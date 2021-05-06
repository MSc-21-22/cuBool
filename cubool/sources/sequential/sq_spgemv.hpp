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

#ifndef CUBOOL_SQ_SPGEMV_HPP
#define CUBOOL_SQ_SPGEMV_HPP

#include <sequential/sq_data.hpp>

namespace cubool {

    /**
     * Matrix-vector multiplication of `a` and `b`.
     *
     * @param a Input matrix
     * @param b Input vector
     * @param[out] out Where to store result
     */
    void sq_spgemv(const CsrData& a, const VecData& b, VecData& out);

    /**
     * Matrix(^T)-vector multiplication of `a` and `b`.
     *
     * @param a Input matrix
     * @param b Input vector
     * @param[out] out Where to store result
     */
    void sq_spgemv_transposed(const CsrData& a, const VecData& b, VecData& out);

}

#endif //CUBOOL_SQ_SPGEMV_HPP
