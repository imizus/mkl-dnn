/*******************************************************************************
* Copyright 2016-2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <utility>
#include <numeric>

#include "gtest/gtest.h"
#include "mkldnn_test_common.hpp"

#include "mkldnn.hpp"

namespace mkldnn {

template <typename data_i_t, typename data_o_t>
inline void check_reorder(const memory::desc &md_i, const memory::desc &md_o,
        const data_i_t *src, const data_o_t *dst)
{
    const int ndims = md_i.data.ndims;
    const int *dims = md_i.data.dims;
    const size_t nelems = std::accumulate(
            dims, dims + ndims, size_t(1), std::multiplies<size_t>());

    for (size_t i = 0; i < nelems; ++i) {
        data_i_t s_raw = src[map_index(md_i, i)];
        data_o_t s = static_cast<data_o_t>(s_raw);
        data_o_t d = dst[map_index(md_o, i)];
        ASSERT_EQ(s, d) << "mismatch at position " << i;
    }
}

template <typename reorder_types>
struct test_simple_params {
    engine::kind engine_kind;
    memory::format fmt_i;
    memory::format fmt_o;
    memory::dims dims;
    bool expect_to_fail;
    mkldnn_status_t expected_status;
};

template <typename reorder_types>
class reorder_simple_test:
    public ::testing::TestWithParam<test_simple_params<reorder_types>>
{
protected:
    virtual void SetUp() {
        using data_i_t = typename reorder_types::first_type;
        using data_o_t = typename reorder_types::second_type;

        test_simple_params<reorder_types> p
            = ::testing::TestWithParam<decltype(p)>::GetParam();

        ASSERT_TRUE(p.engine_kind == engine::kind::cpu);
        auto eng = engine(p.engine_kind, 0);

        const size_t nelems_i = std::accumulate(p.dims.begin(), p.dims.end(),
                size_t(1), std::multiplies<size_t>());
        const size_t nelems_o = std::accumulate(p.dims.begin(), p.dims.end(),
                size_t(1), std::multiplies<size_t>());
        ASSERT_EQ(nelems_i, nelems_o);

        auto src_data = new data_i_t[nelems_i];
        auto dst_data = new data_o_t[nelems_o];

        memory::data_type prec_i = data_traits<data_i_t>::data_type;
        memory::data_type prec_o = data_traits<data_o_t>::data_type;
        auto mpd_i = memory::primitive_desc({p.dims, prec_i, p.fmt_i},
                eng);
        auto mpd_o = memory::primitive_desc({p.dims, prec_o, p.fmt_o},
                eng);

        /* initialize input data */
        for (size_t i = 0; i < nelems_i; ++i)
            src_data[map_index(mpd_i.desc(), i)] = data_i_t(i);

        auto src = memory(mpd_i, src_data);
        auto dst = memory(mpd_o, dst_data);

        auto test = [&]() {
            auto r = reorder(src, dst);
            stream(stream::kind::lazy).submit({r}).wait();
        };

        if (catch_expected_failures(test, p.expect_to_fail, p.expected_status))
            return;

        check_reorder(mpd_i.desc(), mpd_o.desc(), src_data, dst_data);

        delete[] src_data;
        delete[] dst_data;
    }
};

using f32_f32 = std::pair<float, float>;
using s32_s32 = std::pair<int32_t, int32_t>;
using s16_s16 = std::pair<int16_t, int16_t>;
using s8_s8 = std::pair<int8_t, int8_t>;

using reorder_simple_expected_fail_f32_f32 = reorder_simple_test<f32_f32>;
using reorder_simple_test_data_f32_f32 = reorder_simple_test<f32_f32>;
using reorder_simple_test_weights_f32_f32 = reorder_simple_test<f32_f32>;
using reorder_simple_test_weights_f32_f32_IOhw16o16i = reorder_simple_test<f32_f32>;
using reorder_simple_test_s32_s32 = reorder_simple_test<s32_s32>;
using reorder_simple_test_s16_s16 = reorder_simple_test<s16_s16>;
using reorder_simple_test_s8_s8 = reorder_simple_test<s8_s8>;

using eng = engine::kind;
using fmt = memory::format;

using test_simple_params_s32_s32 = test_simple_params<s32_s32>;
using test_simple_params_f32_f32 = test_simple_params<f32_f32>;
using test_simple_params_s16_s16 = test_simple_params<s16_s16>;
using test_simple_params_s8_s8 = test_simple_params<s8_s8>;

using cfg_f32= test_simple_params_f32_f32;
using cfg_s32= test_simple_params_s32_s32;
using cfg_s16= test_simple_params_s16_s16;
using cfg_s8= test_simple_params_s8_s8;

TEST_P(reorder_simple_expected_fail_f32_f32, TestsReorder) { }
INSTANTIATE_TEST_CASE_P(TestReorder, reorder_simple_expected_fail_f32_f32,
        ::testing::Values(
            cfg_f32{eng::cpu, fmt::nchw, fmt::nchw, {0, 16, 8, 8},
                true, mkldnn_invalid_arguments},
            cfg_f32{eng::cpu, fmt::nchw, fmt::nChw8c, {0, 16, 8, 8},
                true, mkldnn_invalid_arguments},
            cfg_f32{eng::cpu, fmt::nchw, fmt::nChw16c, {0, 16, 8, 8},
                true, mkldnn_invalid_arguments},
            cfg_f32{eng::cpu, fmt::OIhw8o8i, fmt::oihw, {32, 0, 3, 3},
                true, mkldnn_invalid_arguments},
            cfg_f32{eng::cpu, fmt::OIhw8i8o, fmt::OIhw8o8i, {0, 32, 3, 3},
                true, mkldnn_invalid_arguments},
            cfg_f32{eng::cpu, fmt::OIhw16o16i, fmt::oihw, {32, 32, 0, 3},
                true, mkldnn_invalid_arguments},
            cfg_f32{eng::cpu, fmt::OIhw16i16o, fmt::OIhw16o16i, {32, 32, 3, 0},
                true, mkldnn_invalid_arguments}
            )
        );

TEST_P(reorder_simple_test_data_f32_f32, TestsReorder) { }
INSTANTIATE_TEST_CASE_P(TestReorder, reorder_simple_test_data_f32_f32,
        ::testing::Values(
            cfg_f32{eng::cpu, fmt::nchw, fmt::nchw, {10, 10, 13, 13}},
            cfg_f32{eng::cpu, fmt::nchw, fmt::nhwc, {10, 10, 10, 10}},
            cfg_f32{eng::cpu, fmt::nhwc, fmt::nchw, {10, 10, 10, 10}},
            cfg_f32{eng::cpu, fmt::nchw, fmt::chwn, {28, 3, 10, 10}},
            cfg_f32{eng::cpu, fmt::chwn, fmt::nchw, {28, 3, 10, 10}},
            cfg_f32{eng::cpu, fmt::nhwc, fmt::nhwc, {10, 10, 13, 13}},
            cfg_f32{eng::cpu, fmt::nchw, fmt::nChw8c, {2, 32, 4, 4}},
            cfg_f32{eng::cpu, fmt::nChw8c, fmt::nchw, {2, 32, 4, 4}},
            cfg_f32{eng::cpu, fmt::chwn, fmt::nChw8c, {28, 96, 10, 10}},
            cfg_f32{eng::cpu, fmt::nChw8c, fmt::chwn, {28, 96, 10, 10}},
            cfg_f32{eng::cpu, fmt::nhwc, fmt::nChw8c, {3, 64, 16, 16}},
            cfg_f32{eng::cpu, fmt::nChw8c, fmt::nhwc, {3, 64, 16, 16}},
            cfg_f32{eng::cpu, fmt::nChw8c, fmt::nChw16c, {10, 96, 27, 27}},
            cfg_f32{eng::cpu, fmt::nChw16c, fmt::nChw8c, {10, 96, 27, 27}},
            cfg_f32{eng::cpu, fmt::nchw, fmt::nChw16c, {2, 64, 4, 4}},
            cfg_f32{eng::cpu, fmt::nChw16c, fmt::nchw, {2, 64, 4, 4}},
            cfg_f32{eng::cpu, fmt::chwn, fmt::nChw16c, {28, 96, 10, 10}},
            cfg_f32{eng::cpu, fmt::nChw16c, fmt::chwn, {28, 96, 10, 10}},
            cfg_f32{eng::cpu, fmt::nhwc, fmt::nChw16c, {2, 64, 4, 4}},
            cfg_f32{eng::cpu, fmt::nChw16c, fmt::nhwc, {2, 64, 4, 4}}
            )
        );

TEST_P(reorder_simple_test_weights_f32_f32, TestsReorder) { }
INSTANTIATE_TEST_CASE_P(TestReorder, reorder_simple_test_weights_f32_f32,
        ::testing::Values(
            cfg_f32{eng::cpu, fmt::hwio, fmt::oihw, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::oihw, fmt::hwio, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::hwio, fmt::Ohwi8o, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::Ohwi8o, fmt::hwio, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::hwio, fmt::Ohwi16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::Ohwi16o, fmt::hwio, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::oihw, fmt::OIhw8i8o, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw8i8o, fmt::oihw, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::ihwo, fmt::OIhw8i8o, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw8i8o, fmt::ihwo, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::oihw, fmt::OIhw8o8i, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw8o8i, fmt::oihw, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw8i8o, fmt::OIhw8o8i, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw8o8i, fmt::OIhw8i8o, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::hwio, fmt::OIhw8i8o, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw8i8o, fmt::hwio, {32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::goihw, fmt::hwigo, {2, 32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::hwigo, fmt::goihw, {2, 32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::goihw, fmt::gOIhw8i8o, {2, 32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw8i8o, fmt::goihw, {2, 32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::goihw, fmt::gOIhw8o8i, {2, 32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw8o8i, fmt::goihw, {2, 32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw8i8o, fmt::gOIhw8o8i, {2, 32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw8o8i, fmt::gOIhw8i8o, {2, 32, 32, 3, 3}},
            cfg_f32{eng::cpu, fmt::oihw, fmt::OIhw16i16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw16i16o, fmt::oihw, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::ihwo, fmt::OIhw16i16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw16i16o, fmt::ihwo, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::oihw, fmt::OIhw16o16i, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw16o16i, fmt::oihw, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::hwio, fmt::OIhw16i16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw16i16o, fmt::hwio, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::goihw, fmt::gOIhw16i16o, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw16i16o, fmt::goihw, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::goihw, fmt::gOIhw16o16i, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw16o16i, fmt::goihw, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw16i16o, fmt::OIhw16o16i, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw16o16i, fmt::OIhw16i16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw16i16o, fmt::gOIhw16o16i, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw16o16i, fmt::gOIhw16i16o, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::oihw, fmt::Oihw16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::Oihw16o, fmt::oihw, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::goihw, fmt::gOihw16o, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOihw16o, fmt::goihw, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::Ohwi16o, fmt::Oihw16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::Oihw16o, fmt::Ohwi16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOhwi16o, fmt::gOihw16o, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOihw16o, fmt::gOhwi16o, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::goihw, fmt::Goihw8g, {16, 16, 16, 3, 3}},
            cfg_f32{eng::cpu, fmt::Goihw8g, fmt::goihw, {16, 16, 16, 3, 3}}
            )
        );

TEST_P(reorder_simple_test_weights_f32_f32_IOhw16o16i, TestsReorder) { }
INSTANTIATE_TEST_CASE_P(TestReorder, reorder_simple_test_weights_f32_f32_IOhw16o16i,
        ::testing::Values(
            cfg_f32{eng::cpu, fmt::oihw, fmt::IOhw16o16i, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::IOhw16o16i, fmt::oihw, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::OIhw16i16o, fmt::IOhw16o16i, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::IOhw16o16i, fmt::OIhw16i16o, {64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::goihw, fmt::gOIhw16o16i, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gIOhw16o16i, fmt::goihw, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gOIhw16i16o, fmt::gIOhw16o16i, {2, 64, 64, 3, 3}},
            cfg_f32{eng::cpu, fmt::gIOhw16o16i, fmt::gOIhw16i16o, {2, 64, 64, 3, 3}}
            )
        );


TEST_P(reorder_simple_test_s32_s32, TestsReorder) { }
INSTANTIATE_TEST_CASE_P(TestReorder, reorder_simple_test_s32_s32,
        ::testing::Values(
            cfg_s32{eng::cpu, fmt::nchw, fmt::nChw16c, {2, 64, 4, 4}},
            cfg_s32{eng::cpu, fmt::nChw16c, fmt::nchw, {2, 64, 4, 4}}
            )
        );

TEST_P(reorder_simple_test_s16_s16, TestsReorder) { }
INSTANTIATE_TEST_CASE_P(TestReorder, reorder_simple_test_s16_s16,
        ::testing::Values(
            cfg_s16{eng::cpu, fmt::oihw, fmt::OIhw8i16o2i, {64, 64, 3, 3}},
            cfg_s16{eng::cpu, fmt::OIhw8i16o2i, fmt::oihw, {64, 64, 3, 3}},
            cfg_s16{eng::cpu, fmt::goihw, fmt::gOIhw8i16o2i, {2, 64, 64, 3, 3}},
            cfg_s16{eng::cpu, fmt::gOIhw8i16o2i, fmt::goihw, {2, 64, 64, 3, 3}},
            cfg_s16{eng::cpu, fmt::OIhw8i16o2i, fmt::OIhw8o16i2o, {64, 64, 3, 3}},
            cfg_s16{eng::cpu, fmt::OIhw8o16i2o, fmt::OIhw8i16o2i, {64, 64, 3, 3}},
            cfg_s16{eng::cpu, fmt::gOIhw8i16o2i, fmt::gOIhw8o16i2o, {2, 64, 64, 3, 3}},
            cfg_s16{eng::cpu, fmt::gOIhw8o16i2o, fmt::gOIhw8i16o2i, {2, 64, 64, 3, 3}}
            )
        );

TEST_P(reorder_simple_test_s8_s8, TestsReorder) { }
INSTANTIATE_TEST_CASE_P(TestReorder, reorder_simple_test_s8_s8,
        ::testing::Values(
            cfg_s8{eng::cpu, fmt::oihw, fmt::OIhw4i16o4i, {64, 64, 3, 3}},
            cfg_s8{eng::cpu, fmt::OIhw4i16o4i, fmt::oihw, {64, 64, 3, 3}},
            cfg_s8{eng::cpu, fmt::goihw, fmt::gOIhw4i16o4i, {2, 64, 64, 3, 3}},
            cfg_s8{eng::cpu, fmt::gOIhw4i16o4i, fmt::goihw, {2, 64, 64, 3, 3}}
            )
        );
}
