#include "mlir_biquad.h"

#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/SCF/IR/SCF.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/ExecutionEngine/ExecutionEngine.h>
#include <mlir/ExecutionEngine/OptUtils.h>
#include <mlir/Target/LLVMIR/Dialect/All.h>
#include <mlir/Conversion/ReconcileUnrealizedCasts/ReconcileUnrealizedCasts.h>
#include <mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h>
#include <mlir/Conversion/ArithToLLVM/ArithToLLVM.h>
#include <mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h>
#include <mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/Passes.h>
#include <mlir/Dialect/Affine/Passes.h>

#include <llvm/Support/TargetSelect.h>

#include <memory>

using namespace mlir;

// JIT context structure
struct MLIRBiQuadJIT {
    std::unique_ptr<MLIRContext> context;
    std::unique_ptr<ExecutionEngine> engine;

    // Function pointer for single sample processing
    // Signature: (a0, a1, a2, b1, b2, input, xz1, xz2, yz1, yz2) -> yn
    typedef double (*BiQuadProcessFn)(double a0, double a1, double a2,
                                      double b1, double b2,
                                      double input,
                                      double xz1, double xz2,
                                      double yz1, double yz2);
    BiQuadProcessFn process_fn;

    // Function pointer for buffer processing
    // Signature: (input_ptr, output_ptr, length, a0, a1, a2, b1, b2,
    //             state_ptr[xz1, xz2, yz1, yz2])
    typedef void (*BiQuadProcessBufferFn)(const double *input, double *output,
                                          int64_t length,
                                          double a0, double a1, double a2,
                                          double b1, double b2,
                                          double *state);
    BiQuadProcessBufferFn process_buffer_fn;

    MLIRBiQuadJIT() : context(nullptr), engine(nullptr),
                      process_fn(nullptr), process_buffer_fn(nullptr) {}
};

// Generate MLIR IR for BiQuad difference equation
// yn = a0*input + a1*xz1 + a2*xz2 - b1*yz1 - b2*yz2
static OwningOpRef<ModuleOp> createBiQuadModule(MLIRContext *context) {
    OpBuilder builder(context);
    auto loc = builder.getUnknownLoc();

    // Create module
    auto module = ModuleOp::create(loc);
    builder.setInsertionPointToEnd(module.getBody());

    // Build function type: (f64, f64, f64, f64, f64, f64, f64, f64, f64, f64) -> f64
    auto f64Type = builder.getF64Type();

    SmallVector<Type, 10> argTypes(10, f64Type);
    auto funcType = builder.getFunctionType(argTypes, f64Type);

    // Create function
    auto func = builder.create<func::FuncOp>(loc, "biquad_process", funcType);
    func.setPublic();

    // Create entry block
    auto &entryBlock = *func.addEntryBlock();
    builder.setInsertionPointToStart(&entryBlock);

    // Get arguments
    Value a0 = entryBlock.getArgument(0);
    Value a1 = entryBlock.getArgument(1);
    Value a2 = entryBlock.getArgument(2);
    Value b1 = entryBlock.getArgument(3);
    Value b2 = entryBlock.getArgument(4);
    Value input = entryBlock.getArgument(5);
    Value xz1 = entryBlock.getArgument(6);
    Value xz2 = entryBlock.getArgument(7);
    Value yz1 = entryBlock.getArgument(8);
    Value yz2 = entryBlock.getArgument(9);

    // Implement BiQuad difference equation:
    // yn = a0*input + a1*xz1 + a2*xz2 - b1*yz1 - b2*yz2

    // Feedforward terms (positive)
    auto t1 = builder.create<arith::MulFOp>(loc, a0, input);
    auto t2 = builder.create<arith::MulFOp>(loc, a1, xz1);
    auto t3 = builder.create<arith::MulFOp>(loc, a2, xz2);

    // Feedback terms (negative)
    auto t4 = builder.create<arith::MulFOp>(loc, b1, yz1);
    auto t5 = builder.create<arith::MulFOp>(loc, b2, yz2);

    // Sum feedforward terms
    auto s1 = builder.create<arith::AddFOp>(loc, t1, t2);
    auto s2 = builder.create<arith::AddFOp>(loc, s1, t3);

    // Subtract feedback terms
    auto s3 = builder.create<arith::SubFOp>(loc, s2, t4);
    auto yn = builder.create<arith::SubFOp>(loc, s3, t5);

    // Return output
    builder.create<func::ReturnOp>(loc, yn.getResult());

    return module;
}

// Generate MLIR IR for buffer-level BiQuad processing
// Processes entire buffer in one JIT call, eliminating per-sample overhead
static void addBufferProcessFunction(ModuleOp module, MLIRContext *context) {
    OpBuilder builder(context);
    auto loc = builder.getUnknownLoc();
    builder.setInsertionPointToEnd(module.getBody());

    // Build function type for buffer processing:
    // func @biquad_process_buffer(
    //     input: memref<?xf64>, output: memref<?xf64>, length: i64,
    //     a0: f64, a1: f64, a2: f64, b1: f64, b2: f64,
    //     state: memref<4xf64>  // [xz1, xz2, yz1, yz2]
    // )

    auto f64Type = builder.getF64Type();
    auto i64Type = builder.getI64Type();
    auto ptrType = LLVM::LLVMPointerType::get(context);

    SmallVector<Type, 10> argTypes;
    argTypes.push_back(ptrType);  // input pointer
    argTypes.push_back(ptrType);  // output pointer
    argTypes.push_back(i64Type);  // length
    argTypes.push_back(f64Type);  // a0
    argTypes.push_back(f64Type);  // a1
    argTypes.push_back(f64Type);  // a2
    argTypes.push_back(f64Type);  // b1
    argTypes.push_back(f64Type);  // b2
    argTypes.push_back(ptrType);  // state pointer

    auto funcType = builder.getFunctionType(argTypes, {});

    // Create function
    auto func = builder.create<func::FuncOp>(loc, "biquad_process_buffer", funcType);
    func.setPublic();

    // Create entry block
    auto &entryBlock = *func.addEntryBlock();
    builder.setInsertionPointToStart(&entryBlock);

    // Get arguments
    Value inputPtr = entryBlock.getArgument(0);
    Value outputPtr = entryBlock.getArgument(1);
    Value length = entryBlock.getArgument(2);
    Value a0 = entryBlock.getArgument(3);
    Value a1 = entryBlock.getArgument(4);
    Value a2 = entryBlock.getArgument(5);
    Value b1 = entryBlock.getArgument(6);
    Value b2 = entryBlock.getArgument(7);
    Value statePtr = entryBlock.getArgument(8);

    // Load initial state: xz1, xz2, yz1, yz2
    auto zero = builder.create<arith::ConstantOp>(loc, i64Type, builder.getI64IntegerAttr(0));
    auto one = builder.create<arith::ConstantOp>(loc, i64Type, builder.getI64IntegerAttr(1));
    auto two = builder.create<arith::ConstantOp>(loc, i64Type, builder.getI64IntegerAttr(2));
    auto three = builder.create<arith::ConstantOp>(loc, i64Type, builder.getI64IntegerAttr(3));

    auto xz1Ptr = builder.create<LLVM::GEPOp>(loc, ptrType, f64Type, statePtr, ValueRange{zero});
    auto xz2Ptr = builder.create<LLVM::GEPOp>(loc, ptrType, f64Type, statePtr, ValueRange{one});
    auto yz1Ptr = builder.create<LLVM::GEPOp>(loc, ptrType, f64Type, statePtr, ValueRange{two});
    auto yz2Ptr = builder.create<LLVM::GEPOp>(loc, ptrType, f64Type, statePtr, ValueRange{three});

    auto xz1 = builder.create<LLVM::LoadOp>(loc, f64Type, xz1Ptr);
    auto xz2 = builder.create<LLVM::LoadOp>(loc, f64Type, xz2Ptr);
    auto yz1 = builder.create<LLVM::LoadOp>(loc, f64Type, yz1Ptr);
    auto yz2 = builder.create<LLVM::LoadOp>(loc, f64Type, yz2Ptr);

    // Create loop
    auto loopZero = builder.create<arith::ConstantOp>(loc, i64Type, builder.getI64IntegerAttr(0));
    auto loopOne = builder.create<arith::ConstantOp>(loc, i64Type, builder.getI64IntegerAttr(1));

    // Create loop: for (i = 0; i < length; i++)
    auto loop = builder.create<scf::ForOp>(loc, loopZero, length, loopOne,
                                           ValueRange{xz1, xz2, yz1, yz2});

    builder.setInsertionPointToStart(loop.getBody());
    Value i = loop.getInductionVar();
    Value loopXz1 = loop.getRegionIterArgs()[0];
    Value loopXz2 = loop.getRegionIterArgs()[1];
    Value loopYz1 = loop.getRegionIterArgs()[2];
    Value loopYz2 = loop.getRegionIterArgs()[3];

    // Load input[i]
    auto inputElemPtr = builder.create<LLVM::GEPOp>(loc, ptrType, f64Type, inputPtr, ValueRange{i});
    auto input = builder.create<LLVM::LoadOp>(loc, f64Type, inputElemPtr);

    // BiQuad computation: yn = a0*input + a1*xz1 + a2*xz2 - b1*yz1 - b2*yz2
    auto t1 = builder.create<arith::MulFOp>(loc, a0, input);
    auto t2 = builder.create<arith::MulFOp>(loc, a1, loopXz1);
    auto t3 = builder.create<arith::MulFOp>(loc, a2, loopXz2);
    auto t4 = builder.create<arith::MulFOp>(loc, b1, loopYz1);
    auto t5 = builder.create<arith::MulFOp>(loc, b2, loopYz2);

    auto s1 = builder.create<arith::AddFOp>(loc, t1, t2);
    auto s2 = builder.create<arith::AddFOp>(loc, s1, t3);
    auto s3 = builder.create<arith::SubFOp>(loc, s2, t4);
    auto yn = builder.create<arith::SubFOp>(loc, s3, t5);

    // Store output[i] = yn
    auto outputElemPtr = builder.create<LLVM::GEPOp>(loc, ptrType, f64Type, outputPtr, ValueRange{i});
    builder.create<LLVM::StoreOp>(loc, yn, outputElemPtr);

    // Update state: new_xz2 = xz1, new_xz1 = input, new_yz2 = yz1, new_yz1 = yn
    builder.create<scf::YieldOp>(loc, ValueRange{input, loopXz1, yn, loopYz1});

    // After loop, store final state
    builder.setInsertionPointAfter(loop);
    builder.create<LLVM::StoreOp>(loc, loop.getResult(0), xz1Ptr);
    builder.create<LLVM::StoreOp>(loc, loop.getResult(1), xz2Ptr);
    builder.create<LLVM::StoreOp>(loc, loop.getResult(2), yz1Ptr);
    builder.create<LLVM::StoreOp>(loc, loop.getResult(3), yz2Ptr);

    builder.create<func::ReturnOp>(loc);
}

// Lower MLIR to LLVM dialect and create execution engine
static std::unique_ptr<ExecutionEngine> createExecutionEngine(
    OwningOpRef<ModuleOp> &module, MLIRContext *context) {

    // Create pass manager for lowering (translations already registered)
    PassManager pm(context);

    // Add optimization and lowering passes
    pm.addPass(createCanonicalizerPass());

    // Loop optimizations (unroll by factor of 4 for better ILP)
    pm.addNestedPass<func::FuncOp>(mlir::affine::createLoopUnrollPass(4));

    // Convert high-level dialects to LLVM dialect
    pm.addPass(createConvertSCFToCFPass());  // SCF -> ControlFlow
    pm.addPass(createArithToLLVMConversionPass());  // Arith -> LLVM
    pm.addPass(createConvertControlFlowToLLVMPass());  // ControlFlow -> LLVM
    pm.addPass(createConvertFuncToLLVMPass());  // Func -> LLVM
    pm.addPass(createReconcileUnrealizedCastsPass());

    // Run passes
    if (failed(pm.run(module.get()))) {
        fprintf(stderr, "Pass manager failed\n");
        return nullptr;
    }

    // Debug: Print module after lowering
    // fprintf(stderr, "After lowering:\n");
    // module->dump();

    // Initialize LLVM targets for JIT
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Create execution engine with optimization level 3
    ExecutionEngineOptions options;
    auto transformer = mlir::makeOptimizingTransformer(
        3,  // Optimization level (0-3)
        0,  // Size level
        nullptr);

    options.transformer = transformer;

    auto engine = ExecutionEngine::create(module.get(), options);
    if (!engine) {
        return nullptr;
    }

    return std::move(*engine);
}

extern "C" {

MLIRBiQuadJIT* mlir_biquad_jit_create(const BiQuad *bq) {
    if (!bq) {
        return nullptr;
    }

    auto jit = new MLIRBiQuadJIT();

    // Create MLIR context
    jit->context = std::make_unique<MLIRContext>();

    // Register all dialect translations FIRST
    DialectRegistry registry;
    registerAllToLLVMIRTranslations(registry);
    jit->context->appendDialectRegistry(registry);

    // Now load required dialects
    jit->context->getOrLoadDialect<func::FuncDialect>();
    jit->context->getOrLoadDialect<arith::ArithDialect>();
    jit->context->getOrLoadDialect<scf::SCFDialect>();
    jit->context->getOrLoadDialect<LLVM::LLVMDialect>();

    // Generate MLIR module for BiQuad processing
    auto module = createBiQuadModule(jit->context.get());

    // Add buffer processing function
    addBufferProcessFunction(module.get(), jit->context.get());

    // Verify module
    if (module->verify().failed()) {
        fprintf(stderr, "Module verification failed\n");
        delete jit;
        return nullptr;
    }

    // Debug: Print module before lowering
    // module->dump();

    // Create execution engine and JIT compile
    jit->engine = createExecutionEngine(module, jit->context.get());
    if (!jit->engine) {
        delete jit;
        return nullptr;
    }

    // Lookup JIT-compiled function
    // Use lookup (not lookupPacked) for proper C calling convention
    auto maybeFn = jit->engine->lookup("biquad_process");
    if (!maybeFn) {
        fprintf(stderr, "Failed to lookup biquad_process function\n");
        delete jit;
        return nullptr;
    }

    jit->process_fn = reinterpret_cast<MLIRBiQuadJIT::BiQuadProcessFn>(*maybeFn);

    if (!jit->process_fn) {
        fprintf(stderr, "JIT function pointer is null\n");
        delete jit;
        return nullptr;
    }

    // Lookup buffer processing function
    auto maybeBufferFn = jit->engine->lookup("biquad_process_buffer");
    if (maybeBufferFn) {
        jit->process_buffer_fn = reinterpret_cast<MLIRBiQuadJIT::BiQuadProcessBufferFn>(*maybeBufferFn);
    } else {
        fprintf(stderr, "Warning: Buffer processing function not found, falling back to per-sample\n");
    }

    return jit;
}

double mlir_biquad_process(MLIRBiQuadJIT *jit, BiQuad *bq, double input) {
    if (!jit || !jit->process_fn || !bq) {
        fprintf(stderr, "mlir_biquad_process: null pointer check failed\n");
        return 0.0;
    }

    // Call JIT-compiled function with current coefficients and state
    double yn = jit->process_fn(
        bq->a0, bq->a1, bq->a2,
        bq->b1, bq->b2,
        input,
        bq->xz1, bq->xz2,
        bq->yz1, bq->yz2
    );

    // Update delay state manually (MLIR function is pure computation)
    bq->xz2 = bq->xz1;
    bq->xz1 = input;
    bq->yz2 = bq->yz1;
    bq->yz1 = yn;

    // Apply underflow prevention (same as original C code)
    if (yn > 0.0 && yn < FLT_MIN_PLUS) yn = 0.0;
    if (yn < 0.0 && yn > FLT_MIN_MINUS) yn = 0.0;

    return yn;
}

void mlir_biquad_process_buffer(MLIRBiQuadJIT *jit, BiQuad *bq,
                                const double *input, double *output,
                                size_t length) {
    if (!jit || !bq || !input || !output) {
        return;
    }

    // Use JIT-compiled buffer processing if available
    if (jit->process_buffer_fn) {
        // Pack state into array for JIT function
        double state[4] = {bq->xz1, bq->xz2, bq->yz1, bq->yz2};

        // Call JIT-compiled buffer processing
        jit->process_buffer_fn(input, output, (int64_t)length,
                              bq->a0, bq->a1, bq->a2, bq->b1, bq->b2,
                              state);

        // Update BiQuad state from array
        bq->xz1 = state[0];
        bq->xz2 = state[1];
        bq->yz1 = state[2];
        bq->yz2 = state[3];

        // Apply underflow prevention to final state
        if (bq->yz1 > 0.0 && bq->yz1 < FLT_MIN_PLUS) bq->yz1 = 0.0;
        if (bq->yz1 < 0.0 && bq->yz1 > FLT_MIN_MINUS) bq->yz1 = 0.0;
    } else {
        // Fallback: process sample by sample
        for (size_t i = 0; i < length; i++) {
            output[i] = mlir_biquad_process(jit, bq, input[i]);
        }
    }
}

void mlir_biquad_jit_destroy(MLIRBiQuadJIT *jit) {
    if (jit) {
        delete jit;
    }
}

int mlir_biquad_available(void) {
    return 1;  // Always available when compiled with USE_MLIR
}

} // extern "C"
