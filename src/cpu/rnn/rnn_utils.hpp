/*******************************************************************************
* Copyright 2018 Intel Corporation
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

#ifndef RNN_UTILS_HPP
#define RNN_UTILS_HPP

#include "mkldnn.h"

#include "cpu_rnn_pd.hpp"

namespace mkldnn {
namespace impl {
namespace cpu {

namespace rnn_utils {

using namespace mkldnn::impl::utils;

enum execution_direction_t {
    l2r,
    r2l,
    bi_concat,
    bi_sum,
};

enum data_type_conf_t {
    all_f32,
    u8u8u8f32,
    f32u8f32f32,
    u8u8u8u8,
    f32u8f32u8
};

struct rnn_conf_t {
    execution_direction_t exec_dir;
    data_type_conf_t dt_conf;
    int n_layer, n_iter, n_dir, n_gates, n_states;
    int mb;
    int slc, sic, dic, dlc;
    int gates_ld, gates_nld, gates_ws_ld;
    int n_parts_weights_layer, parts_weights_layer[MKLDNN_RNN_MAX_N_PARTS];
    int n_parts_weights_iter, parts_weights_iter[MKLDNN_RNN_MAX_N_PARTS];
    int n_bias, n_parts_bias, parts_bias[MKLDNN_RNN_MAX_N_PARTS];
    size_t part_weights_iter_pack_size[MKLDNN_RNN_MAX_N_PARTS],
            part_weights_layer_pack_size[MKLDNN_RNN_MAX_N_PARTS];
    bool weights_layer_is_packed, weights_iter_is_packed;
    bool pack_weights_layer, pack_weights_iter;
    /* Size of packed data in bytes */
    size_t weights_layer_comp_offset, weights_layer_pack_size,
        weights_iter_comp_offset, weights_iter_pack_size;

    bool copy_weights_layer, copy_weights_iter, copy_diff_weights_layer,
            copy_diff_weights_iter, copy_bias;
    int weights_layer_ld, weights_layer_nld, weights_layer_ws_ld;
    int diff_weights_layer_ld, diff_weights_layer_nld, diff_weights_layer_ws_ld;
    int weights_iter_ld, weights_iter_nld, weights_iter_ws_ld;
    int diff_weights_iter_ld, diff_weights_iter_nld, diff_weights_iter_ws_ld;
    int states_nld, states_ws_ld;
    int weights_iter_compensation_size, weights_layer_compensation_size;
    bool is_fwd, is_training, is_lbr;
    bool use_workspace;

    /* Size of workspace for each tensor in bytes */
    size_t ws_gates_size, ws_states_size, ws_c_states_size, ws_diff_states_size,
            ws_weights_layer_size, ws_weights_iter_size,
            ws_diff_weights_layer_size, ws_diff_weights_iter_size,
            ws_cell_comp_size, ws_grid_comp_size, ws_per_cell, ws_bias_size;
    bool merge_gemm_iter, merge_gemm_layer, use_jit_gemm, use_layer_packed_gemm,
        use_iter_packed_gemm;
    memory_format_t weights_layer_fmt, weights_iter_fmt, diff_weights_layer_fmt,
            diff_weights_iter_fmt;
};

int get_good_ld(int dim, int sizeof_dt);

void init_conf(rnn_conf_t &rnn, const rnn_desc_t &rd,
        const memory_desc_wrapper &src_layer_d,
        const memory_desc_wrapper &src_iter_d,
        const memory_desc_wrapper &weights_layer_d,
        const memory_desc_wrapper &weights_iter_d,
        const memory_desc_wrapper &dst_layer_d);

void set_conf(rnn_conf_t &rnn, const rnn_desc_t &rd,
        const memory_desc_wrapper &weights_layer_d,
        const memory_desc_wrapper &weights_iter_d,
        const memory_desc_wrapper &diff_weights_layer_d,
        const memory_desc_wrapper &diff_weights_iter_d);

void set_offsets(const rnn_conf_t &rnn, size_t &ws_gates_offset,
        size_t &ws_h_state_offset, size_t &ws_c_state_offset,
        size_t &ws_diff_states_offset, size_t &ws_grid_comp_offset,
        size_t &ws_cell_comp_offset, size_t &ws_weights_layer_offset,
        size_t &ws_weights_iter_offset, size_t &ws_bias_offset,
        size_t &ws_diff_weights_layer_offset,
        size_t &ws_diff_weights_iter_offset, size_t &scratchpad_size,
        size_t &workspace_size);

void get_scratchpad_and_workspace_sizes(const rnn_conf_t &rnn,
        size_t &scratchpad_size, size_t &workspace_size);
status_t set_expected_desc(
        rnn_conf_t &rnn, memory_desc_t &weights_md, bool is_iter);

template <typename T>
struct ws_gates_aoc {
    ws_gates_aoc(const rnn_conf_t &rnn, T *data)
        : gates_(data, rnn.gates_nld, rnn.gates_ws_ld), DIC_(rnn.dic) {}
    T &operator()(int batch, int gate, int dic) {
        return gates_(batch, gate * DIC_ + dic);
    }

private:
    mkldnn::impl::utils::array_offset_calculator<T, 2> gates_;
    int DIC_;
};
using ws_gates_aoc_t = ws_gates_aoc<float>;
using ws_gates_aoc_s32_t = ws_gates_aoc<int32_t>;

struct bias_aoc_t {
    bias_aoc_t(const rnn_conf_t &rnn, const float *data)
        : bias_(data, rnn.n_bias, rnn.dic) {}
    const float &operator()(int bias_n, int dic) { return bias_(bias_n, dic); }

private:
    mkldnn::impl::utils::array_offset_calculator<const float, 2> bias_;
};

template <typename T>
struct ws_states_aoc {
    ws_states_aoc(const rnn_conf_t &rnn, T *data)
        : state_(data, rnn.states_nld, rnn.states_ws_ld) {}
    T &operator()(int batch, int dic) { return state_(batch, dic); }

private:
    mkldnn::impl::utils::array_offset_calculator<T, 2> state_;
};
using ws_states_aoc_t = ws_states_aoc<float>;
using ws_states_aoc_u8_t = ws_states_aoc<uint8_t>;

struct ws_diff_states_aoc_t {
    ws_diff_states_aoc_t(const rnn_conf_t &rnn, float *data)
        : diff_states_(data, rnn.n_states + 1, rnn.n_iter + 1, rnn.states_nld,
                  rnn.states_ws_ld) {}
    float &operator()(int state_n, int batch, int dic) {
        return diff_states_(state_n, 0, batch, dic);
    }

private:
    mkldnn::impl::utils::array_offset_calculator<float, 4> diff_states_;
};

struct ws_diff_w_iter_aoc_t {
    ws_diff_w_iter_aoc_t(const rnn_conf_t &rnn, float *data)
        : diff_weights_iter_(
                  data, rnn.diff_weights_iter_nld, rnn.diff_weights_iter_ws_ld)
        , DIC_(rnn.dic) {}
    float &operator()(int sic, int gate, int dic) {
        return diff_weights_iter_(sic, gate * DIC_ + dic);
    }

private:
    mkldnn::impl::utils::array_offset_calculator<float, 2> diff_weights_iter_;
    int DIC_;
};
}
}
}
}
#endif