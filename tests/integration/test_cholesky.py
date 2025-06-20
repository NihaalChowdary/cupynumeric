# Copyright 2024 NVIDIA Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import numpy as np
import pytest
from utils.comparisons import allclose

import cupynumeric as num

SIZES = [8, 9, 255, 512, 1024]


def test_matrix():
    arr = [[1, -2j], [2j, 5]]
    np_out = np.linalg.cholesky(arr)
    num_out = num.linalg.cholesky(arr)
    assert np.array_equal(np_out, num_out)


def test_array_negative_1dim():
    arr = num.random.randint(0, 9, size=(3,))
    with pytest.raises(ValueError):
        num.linalg.cholesky(arr)


def test_array_negative():
    arr = num.random.randint(0, 9, size=(3, 2, 3))
    expected_exc = ValueError
    with pytest.raises(expected_exc):
        num.linalg.cholesky(arr)
    with pytest.raises(expected_exc):
        np.linalg.cholesky(arr)


def test_diagonal():
    a = num.eye(10) * 10.0
    b = num.linalg.cholesky(a)
    assert allclose(b**2.0, a)


def _get_real_symm_posdef(n):
    a = num.random.rand(n, n)
    return a + a.T + num.eye(n) * n * 2


@pytest.mark.parametrize("n", SIZES)
def test_real(n):
    b = _get_real_symm_posdef(n)
    c = num.linalg.cholesky(b)
    c_np = c.__array__()
    b2 = np.dot(c_np, c_np.T)
    assert allclose(b, b2)


@pytest.mark.parametrize("n", SIZES)
def test_complex(n):
    a = num.random.rand(n, n) + num.random.rand(n, n) * 1.0j
    b = a + a.T.conj() + num.eye(n) * n * 3
    c = num.linalg.cholesky(b)
    c_np = c.__array__()
    b2 = np.dot(c_np, c_np.T.conj())
    assert allclose(b, b2)

    d = num.empty((2, n, n), dtype=num.complex128)
    d[1] = b
    c = num.linalg.cholesky(d[1])
    c_np = c.__array__()
    b2 = np.dot(c_np, c_np.T.conj())
    assert allclose(d[1], b2)


@pytest.mark.parametrize("n", SIZES)
def test_batched_3d(n):
    batch = 4
    a = _get_real_symm_posdef(n)
    multiplier = np.arange(batch) + 1
    a_batched = num.einsum("i,jk->ijk", multiplier, a)
    test_c = num.linalg.cholesky(a_batched)
    for i in range(batch):
        test = test_c[i, :]
        c_np = test.__array__()
        a2 = np.dot(c_np, c_np.T)
        ref_a = multiplier[i] * a
        assert allclose(ref_a, a2)


def test_batched_empty():
    batch = 4
    a = _get_real_symm_posdef(8)
    a_batched = num.einsum("i,jk->ijk", np.arange(batch) + 1, a)
    a_sliced = a_batched[0:0, :, :]
    empty = num.linalg.cholesky(a_sliced)
    assert empty.shape == a_sliced.shape


@pytest.mark.parametrize("n", SIZES)
def test_batched_4d(n):
    batch = 2
    a = _get_real_symm_posdef(n)
    outer = np.einsum("i,j->ij", np.arange(batch) + 1, np.arange(batch) + 1)

    a_batched = num.einsum("ij,kl->ijkl", outer, a)
    test_c = num.linalg.cholesky(a_batched)
    for i in range(batch):
        for j in range(batch):
            test = test_c[i, j, :]
            c_np = test.__array__()
            a2 = np.dot(c_np, c_np.T)
            assert allclose(a_batched[i, j, :, :], a2)


@pytest.mark.parametrize("ddtype", (np.int32, np.uint8, np.int64))
def test_cholesky_integer_input(ddtype: np.dtype) -> None:
    arr = np.array([[4, 2], [2, 5]], dtype=ddtype)
    result_num = num.linalg.cholesky(arr)
    result_np = np.linalg.cholesky(arr)

    assert np.allclose(result_np, result_num)


def test_cholesky_bool_input() -> None:
    arr = np.array([[True, False], [False, True]], dtype=bool)
    result_num = num.linalg.cholesky(arr)
    result_np = np.linalg.cholesky(arr)

    expected = np.eye(2, dtype=np.float64)
    assert np.allclose(result_num, expected)
    assert np.allclose(result_num, result_np)


def test_cholesky_linalgerror() -> None:
    arr_np = np.array([[0, 1], [1, 0]], dtype=np.float64)
    arr_num = num.array(arr_np)
    msg = r"Matrix is not positive definite"
    with pytest.raises(num.linalg.LinAlgError, match=msg):
        num.linalg.cholesky(arr_num)
    with pytest.raises(np.linalg.LinAlgError, match=msg):
        np.linalg.cholesky(arr_np)


if __name__ == "__main__":
    import sys

    sys.exit(pytest.main(sys.argv))
