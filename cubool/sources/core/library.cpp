/**********************************************************************************/
/* MIT License                                                                    */
/*                                                                                */
/* Copyright (c) 2020, 2021 JetBrains-Research                                    */
/*                                                                                */
/* Permission is hereby granted, free of charge, to any person obtaining a copy   */
/* of this software and associated documentation files (the "Software"), to deal  */
/* in the Software without restriction, including without limitation the rights   */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell      */
/* copies of the Software, and to permit persons to whom the Software is          */
/* furnished to do so, subject to the following conditions:                       */
/*                                                                                */
/* The above copyright notice and this permission notice shall be included in all */
/* copies or substantial portions of the Software.                                */
/*                                                                                */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  */
/* SOFTWARE.                                                                      */
/**********************************************************************************/

#include <core/library.hpp>
#include <core/error.hpp>
#include <core/matrix.hpp>
#include <backend/backend_base.hpp>
#include <backend/matrix_base.hpp>
#include <io/logger.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <iomanip>

#ifdef CUBOOL_WITH_CUDA
#include <cuda/cuda_backend.hpp>
#endif

#ifdef CUBOOL_WITH_SEQUENTIAL
#include <sequential/sq_backend.hpp>
#endif

namespace cubool {

    std::unordered_set<class MatrixBase*> Library::mAllocated;
    std::shared_ptr<class BackendBase> Library::mBackend = nullptr;
    std::shared_ptr<class Logger>  Library::mLogger = std::make_shared<DummyLogger>();
    bool Library::mRelaxedRelease = false;

    void Library::initialize(hints initHints) {
        CHECK_RAISE_CRITICAL_ERROR(mBackend == nullptr, InvalidState, "Library already initialized");

        bool preferCpu = initHints & CUBOOL_HINT_CPU_BACKEND;

        // If user do not force the cpu backend usage
        if (!preferCpu) {
#ifdef CUBOOL_WITH_CUDA
            mBackend = std::make_shared<CudaBackend>();
            mBackend->initialize(initHints);

            // Failed to setup cuda, release backend and go to try cpu
            if (!mBackend->isInitialized()) {
                mBackend = nullptr;
                mLogger->logWarning("Failed to initialize Cuda backend");
            }
#endif
        }

#ifdef CUBOOL_WITH_SEQUENTIAL
        if (mBackend == nullptr) {
            mBackend = std::make_shared<SqBackend>();
            mBackend->initialize(initHints);

            // Failed somehow setup
            if (!mBackend->isInitialized()) {
                mBackend = nullptr;
                mLogger->logWarning("Failed to initialize Cpu fallback backend");
            }
        }
#endif

        CHECK_RAISE_ERROR(mBackend != nullptr, BackendError, "Failed to select backend");

        // If initialized, post-init actions
        mRelaxedRelease = initHints & CUBOOL_HINT_RELAXED_FINALIZE;
        logDeviceInfo();
    }

    void Library::finalize() {
        if (mBackend) {
            // Release all allocated resources implicitly
            if (mRelaxedRelease) {
                for (auto m: mAllocated) {
                    std::stringstream s;
                    s << "Implicitly release matrix instance " << m;

                    mLogger->logWarning(s.str());
                    delete m;
                }

                mAllocated.clear();
            }

            // Some final message
            mLogger->logInfo("** cuBool:Finalize backend **");

            // Remember to finalize backend
            mBackend->finalize();
            mBackend = nullptr;

            // Release (possibly setup text logger) logger, reassign dummy
            mLogger = std::make_shared<DummyLogger>();
        }
    }

    void Library::validate() {
        CHECK_RAISE_CRITICAL_ERROR(mBackend != nullptr || mRelaxedRelease, InvalidState, "Library is not initialized");
    }

    void Library::setupLogging(const char *logFileName, cuBool_Hints hints) {
        CHECK_RAISE_ERROR(logFileName != nullptr, InvalidArgument, "Null file name is not allowed");

        auto lofFile = std::make_shared<std::ofstream>();

        lofFile->open(logFileName, std::ios::out);

        if (!lofFile->is_open()) {
            RAISE_ERROR(InvalidArgument, "Failed to create logging file");
        }

        // Create logger and setup filters && post-actions
        auto textLogger = std::make_shared<TextLogger>();

        textLogger->addFilter([=](Logger::Level level, const std::string& message) -> bool {
            bool all = hints == 0x0 || (hints & CUBOOL_HINT_LOG_ALL);
            bool error = hints & CUBOOL_HINT_LOG_ERROR;
            bool warning = hints & CUBOOL_HINT_LOG_WARNING;

            return all ||
                    (error && level == Logger::Level::Error) ||
                    (warning && level == Logger::Level::Warning);
        });

        textLogger->addOnLoggerAction([=](size_t id, Logger::Level level, const std::string& message) {
            auto& file = *lofFile;

            const auto idSize = 10;
            const auto levelSize = 20;

            file << "[" << std::setw(idSize) << id << std::setw(-1) << "]";
            file << "[" << std::setw(levelSize);
            switch (level) {
                case Logger::Level::Info:
                    file << "Level::Info";
                    break;
                case Logger::Level::Warning:
                    file << "Level::Warning";
                    break;
                case Logger::Level::Error:
                    file << "Level::Error";
                    break;
                default:
                    file << "Level::Always";
            }
            file << std::setw(-1) << "] ";
            file << message << std::endl;
        });

        // Assign new text logger
        mLogger = textLogger;

        // Initial message
        mLogger->logInfo("*** cuBool::Logger file ***");

        // Also log device capabilities
        if (isBackedInitialized())
            logDeviceInfo();
    }

    MatrixBase *Library::createMatrix(size_t nrows, size_t ncols) {
        CHECK_RAISE_ERROR(nrows > 0, InvalidArgument, "Cannot create matrix with zero dimension");
        CHECK_RAISE_ERROR(ncols > 0, InvalidArgument, "Cannot create matrix with zero dimension");

        auto m = new Matrix(nrows, ncols, *mBackend);
        mAllocated.emplace(m);
        return m;
    }

    void Library::releaseMatrix(MatrixBase *matrixBase) {
        if (mRelaxedRelease && !mBackend) return;

        mAllocated.erase(matrixBase);
        delete matrixBase;
    }

    void Library::handleError(const std::exception& error) {
        mLogger->log(Logger::Level::Error, error.what());
    }

    void Library::queryCapabilities(cuBool_DeviceCaps &caps) {
        caps.name[0] = '\0';
        caps.cudaSupported = false;
        caps.major = 0;
        caps.minor = 0;
        caps.warp = 0;
        caps.globalMemoryKiBs = 0;
        caps.sharedMemoryPerBlockKiBs = 0;
        caps.sharedMemoryPerMultiProcKiBs = 0;

        mBackend->queryCapabilities(caps);
    }

    void Library::logDeviceInfo() {
        // Log device caps
        cuBool_DeviceCaps caps;
        queryCapabilities(caps);

        std::stringstream ss;

        if (caps.cudaSupported) {
            ss << "Cuda device capabilities" << std::endl
               << " name: " << caps.name << std::endl
               << " major: " << caps.major << std::endl
               << " minor: " << caps.minor << std::endl
               << " warp size: " << caps.warp << std::endl
               << " globalMemoryKiBs: " << caps.globalMemoryKiBs << std::endl
               << " sharedMemoryPerMultiProcKiBs: " << caps.sharedMemoryPerMultiProcKiBs << std::endl
               << " sharedMemoryPerBlockKiBs: " << caps.sharedMemoryPerBlockKiBs << std::endl;
        }
        else {
            ss << "Cuda device is not presented";
        }

        mLogger->logInfo(ss.str());
    }

    bool Library::isBackedInitialized() {
        return mBackend != nullptr;
    }

    class Logger * Library::getLogger() {
        return mLogger.get();
    }

}