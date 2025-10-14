// MLIR context implementation
// This file uses C++ because MLIR is a C++ library

#include "mlir_context.h"

#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include <memory>

// Implementation structure (C++ objects)
struct MLIRContextImpl {
    std::unique_ptr<mlir::MLIRContext> context;

    MLIRContextImpl() {
        context = std::make_unique<mlir::MLIRContext>();

        // Register dialects needed for basic functionality
        context->getOrLoadDialect<mlir::func::FuncDialect>();
        context->getOrLoadDialect<mlir::arith::ArithDialect>();
    }

    ~MLIRContextImpl() = default;
};

// C API implementation

extern "C" {

int mlir_context_init(MLIRContextHandle *handle) {
    if (!handle) {
        return -1;
    }

    try {
        handle->impl = new MLIRContextImpl();
        handle->initialized = 1;
        return 0;
    } catch (...) {
        handle->impl = nullptr;
        handle->initialized = 0;
        return -1;
    }
}

void mlir_context_cleanup(MLIRContextHandle *handle) {
    if (handle && handle->impl) {
        delete handle->impl;
        handle->impl = nullptr;
        handle->initialized = 0;
    }
}

int mlir_is_available(void) {
    // If this code compiles and links, MLIR is available
    return 1;
}

const char* mlir_get_version(void) {
    // LLVM/MLIR version - this will be embedded at compile time
    return "20.1.8";
}

int mlir_verify_functionality(MLIRContextHandle *handle) {
    if (!handle || !handle->initialized || !handle->impl) {
        return -1;
    }

    try {
        // Create a simple MLIR module as a functionality test
        mlir::MLIRContext *ctx = handle->impl->context.get();
        mlir::OpBuilder builder(ctx);

        // Create a module
        auto loc = builder.getUnknownLoc();
        auto module = mlir::ModuleOp::create(loc);

        // Verify the module (use module.verify() method in MLIR 20+)
        if (module.verify().failed()) {
            module.erase();
            return -1;
        }

        // Clean up
        module.erase();
        return 0;
    } catch (...) {
        return -1;
    }
}

} // extern "C"
