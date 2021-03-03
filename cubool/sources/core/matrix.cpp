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

#include <core/matrix.hpp>
#include <core/error.hpp>
#include <cassert>
#include <algorithm>
#include <limits>

namespace cubool {

    Matrix::Matrix(size_t nrows, size_t ncols, BackendBase &backend) {
        mHnd = backend.createMatrix(nrows, ncols);
        mProvider = &backend;
    }

    Matrix::~Matrix() {
        if (mHnd) {
            mProvider->releaseMatrix(mHnd);
            mHnd = nullptr;
            mProvider = nullptr;
        }
    }

    void Matrix::setElement(index i, index j) {
        CHECK_RAISE_ERROR(i < getNrows(), InvalidArgument, "Value out of matrix bounds");
        CHECK_RAISE_ERROR(j < getNcols(), InvalidArgument, "Value out of matrix bounds");

        // This values will be committed later
        mCachedI.push_back(i);
        mCachedJ.push_back(j);
    }

    void Matrix::build(const index *rows, const index *cols, size_t nvals, bool isSorted, bool hasDuplicates) {
        CHECK_RAISE_ERROR(rows != nullptr || nvals == 0, InvalidArgument, "Null ptr rows array");
        CHECK_RAISE_ERROR(cols != nullptr || nvals == 0, InvalidArgument, "Null ptr cols array");

        this->releaseCache();
        mHnd->build(rows, cols, nvals, isSorted, hasDuplicates);
    }

    void Matrix::extract(index *rows, index *cols, size_t &nvals) {
        CHECK_RAISE_ERROR(rows != nullptr || getNvals() == 0, InvalidArgument, "Null ptr rows array");
        CHECK_RAISE_ERROR(cols != nullptr || getNvals() == 0, InvalidArgument, "Null ptr cols array");
        CHECK_RAISE_ERROR(getNvals() <= nvals, InvalidArgument, "Passed arrays size must be more or equal to the nvals of the matrix");

        this->commitCache();
        mHnd->extract(rows, cols, nvals);
    }

    void Matrix::extractSubMatrix(const MatrixBase &otherBase, index i, index j, index nrows, index ncols) {
        const auto* other = dynamic_cast<const Matrix*>(&otherBase);

        CHECK_RAISE_ERROR(other != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");

        auto bI = i + nrows;
        auto bJ = j + ncols;

        CHECK_RAISE_ERROR(nrows > 0, InvalidArgument, "Cannot extract sub-matrix with zero dimension");
        CHECK_RAISE_ERROR(ncols > 0, InvalidArgument, "Cannot extract sub-matrix with zero dimension");

        CHECK_RAISE_ERROR(bI <= other->getNrows(), InvalidArgument, "Provided sub-matrix range must be within matrix bounds");
        CHECK_RAISE_ERROR(bJ <= other->getNcols(), InvalidArgument, "Provided sub-matrix range must be within matrix bounds");

        CHECK_RAISE_ERROR(nrows == this->getNrows(), InvalidArgument, "Result matrix has incompatible size for extracted sub-matrix range");
        CHECK_RAISE_ERROR(ncols == this->getNcols(), InvalidArgument, "Result matrix has incompatible size for extracted sub-matrix range");

        this->commitCache();
        mHnd->extractSubMatrix(*other->mHnd, i, j, nrows, ncols);
    }

    void Matrix::clone(const MatrixBase &otherBase) {
        const auto* other = dynamic_cast<const Matrix*>(&otherBase);

        CHECK_RAISE_ERROR(other != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");

        auto M = other->getNrows();
        auto N = other->getNcols();

        CHECK_RAISE_ERROR(M == this->getNrows(), InvalidArgument, "Cloned matrix has incompatible size");
        CHECK_RAISE_ERROR(N == this->getNcols(), InvalidArgument, "Cloned matrix has incompatible size");

        this->commitCache();
        mHnd->clone(*other->mHnd);
    }

    void Matrix::transpose(const MatrixBase &otherBase) {
        const auto* other = dynamic_cast<const Matrix*>(&otherBase);

        CHECK_RAISE_ERROR(other != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");

        auto M = other->getNrows();
        auto N = other->getNcols();

        CHECK_RAISE_ERROR(M == this->getNcols(), InvalidArgument, "Transposed matrix has incompatible size");
        CHECK_RAISE_ERROR(N == this->getNrows(), InvalidArgument, "Transposed matrix has incompatible size");

        this->commitCache();
        mHnd->transpose(*other->mHnd);
    }

    void Matrix::reduce(const MatrixBase &otherBase) {
        const auto* other = dynamic_cast<const Matrix*>(&otherBase);

        CHECK_RAISE_ERROR(other != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");

        auto M = other->getNrows();

        CHECK_RAISE_ERROR(M == this->getNrows(), InvalidArgument, "Matrix has incompatible size");
        CHECK_RAISE_ERROR(1 == this->getNcols(), InvalidArgument, "Matrix has incompatible size");

        this->commitCache();
        mHnd->reduce(*other->mHnd);
    }

    void Matrix::multiply(const MatrixBase &aBase, const MatrixBase &bBase, bool accumulate) {
        const auto* a = dynamic_cast<const Matrix*>(&aBase);
        const auto* b = dynamic_cast<const Matrix*>(&bBase);

        CHECK_RAISE_ERROR(a != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");
        CHECK_RAISE_ERROR(b != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");

        auto M = a->getNrows();
        auto T = a->getNcols();
        auto N = b->getNcols();

        CHECK_RAISE_ERROR(M == this->getNrows(), InvalidArgument, "Matrix has incompatible size for operation result");
        CHECK_RAISE_ERROR(N == this->getNcols(), InvalidArgument, "Matrix has incompatible size for operation result");
        CHECK_RAISE_ERROR(T == b->getNrows(), InvalidArgument, "Cannot multiply passed matrices");

        this->commitCache();
        mHnd->multiply(*a->mHnd, *b->mHnd, accumulate);
    }

    void Matrix::kronecker(const MatrixBase &aBase, const MatrixBase &bBase) {
        const auto* a = dynamic_cast<const Matrix*>(&aBase);
        const auto* b = dynamic_cast<const Matrix*>(&bBase);

        CHECK_RAISE_ERROR(a != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");
        CHECK_RAISE_ERROR(b != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");

        index M = a->getNrows();
        index N = a->getNcols();
        index K = b->getNrows();
        index T = b->getNcols();

        CHECK_RAISE_ERROR(M * K == this->getNrows(), InvalidArgument, "Matrix has incompatible size for operation result");
        CHECK_RAISE_ERROR(N * T == this->getNcols(), InvalidArgument, "Matrix has incompatible size for operation result");

        this->commitCache();
        mHnd->kronecker(*a->mHnd, *b->mHnd);
    }

    void Matrix::eWiseAdd(const MatrixBase &aBase, const MatrixBase &bBase) {
        const auto* a = dynamic_cast<const Matrix*>(&aBase);
        const auto* b = dynamic_cast<const Matrix*>(&bBase);

        CHECK_RAISE_ERROR(a != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");
        CHECK_RAISE_ERROR(b != nullptr, InvalidArgument, "Passed matrix does not belong to core matrix class");

        index M = a->getNrows();
        index N = a->getNcols();

        CHECK_RAISE_ERROR(M == b->getNrows(), InvalidArgument, "Passed matrices have incompatible size");
        CHECK_RAISE_ERROR(N == b->getNcols(), InvalidArgument, "Passed matrices have incompatible size");

        CHECK_RAISE_ERROR(M == this->getNrows(), InvalidArgument, "Matrix has incompatible size for operation result");
        CHECK_RAISE_ERROR(N == this->getNcols(), InvalidArgument, "Matrix has incompatible size for operation result");

        this->commitCache();
        mHnd->eWiseAdd(*a->mHnd, *b->mHnd);
    }

    index Matrix::getNrows() const {
        return mHnd->getNrows();
    }

    index Matrix::getNcols() const {
        return mHnd->getNcols();
    }

    index Matrix::getNvals() const {
        this->commitCache();
        return mHnd->getNvals();
    }

    void Matrix::releaseCache() const {
        mCachedI.clear();
        mCachedJ.clear();
    }

    void Matrix::commitCache() const {
        assert(mCachedI.size() == mCachedJ.size());

        size_t cachedNvals = mCachedI.size();

        // Nothing to do if no value was cached on CPU side
        if (cachedNvals == 0)
            return;

        size_t currentNvals = mHnd->getNvals();

        // Read values from backend matrix and join
        if (currentNvals > 0) {
            mCachedI.resize(cachedNvals + currentNvals);
            mCachedJ.resize(cachedNvals + currentNvals);
            mHnd->extract(mCachedI.data() + cachedNvals, mCachedJ.data() + cachedNvals, currentNvals);
        }

        // Build matrix
        mHnd->build(mCachedI.data(), mCachedJ.data(), cachedNvals + currentNvals, false, true);

        // Clear arrays
        releaseCache();
    }
}