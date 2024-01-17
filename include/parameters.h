// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once
#include <omp.h>
#include <sstream>
#include <typeinfo>
#include <unordered_map>

#include "omp.h"
#include "defaults.h"
#include "timer.h"
#include "windows_customizations.h"
#include "percentile_stats.h"

namespace diskann
{

class IndexWriteParameters

{
  public:
    const uint32_t search_list_size; // L
    const uint32_t max_degree;       // R
    const bool saturate_graph;
    const uint32_t max_occlusion_size; // C
    const float alpha;
    const uint32_t num_threads;
    const uint32_t filter_list_size; // Lf
    const uint32_t num_frozen_points;

  private:
    IndexWriteParameters(const uint32_t search_list_size, const uint32_t max_degree, const bool saturate_graph,
                         const uint32_t max_occlusion_size, const float alpha, const uint32_t num_threads,
                         const uint32_t filter_list_size, const uint32_t num_frozen_points)
        : search_list_size(search_list_size), max_degree(max_degree), saturate_graph(saturate_graph),
          max_occlusion_size(max_occlusion_size), alpha(alpha), num_threads(num_threads),
          filter_list_size(filter_list_size), num_frozen_points(num_frozen_points)
    {
    }

    friend class IndexWriteParametersBuilder;
};

class IndexWriteParametersBuilder
{
    /**
     * Fluent builder pattern to keep track of the 7 non-default properties
     * and their order. The basic ctor was getting unwieldy.
     */
  public:
    IndexWriteParametersBuilder(const uint32_t search_list_size, // L
                                const uint32_t max_degree        // R
                                )
        : _search_list_size(search_list_size), _max_degree(max_degree)
    {
    }

    IndexWriteParametersBuilder &with_max_occlusion_size(const uint32_t max_occlusion_size)
    {
        _max_occlusion_size = max_occlusion_size;
        return *this;
    }

    IndexWriteParametersBuilder &with_saturate_graph(const bool saturate_graph)
    {
        _saturate_graph = saturate_graph;
        return *this;
    }

    IndexWriteParametersBuilder &with_alpha(const float alpha)
    {
        _alpha = alpha;
        return *this;
    }

    IndexWriteParametersBuilder &with_num_threads(const uint32_t num_threads)
    {
        _num_threads = num_threads == 0 ? omp_get_num_threads() : num_threads;
        return *this;
    }

    IndexWriteParametersBuilder &with_filter_list_size(const uint32_t filter_list_size)
    {
        _filter_list_size = filter_list_size == 0 ? _search_list_size : filter_list_size;
        return *this;
    }

    IndexWriteParametersBuilder &with_num_frozen_points(const uint32_t num_frozen_points)
    {
        _num_frozen_points = num_frozen_points;
        return *this;
    }

    IndexWriteParameters build() const
    {
        return IndexWriteParameters(_search_list_size, _max_degree, _saturate_graph, _max_occlusion_size, _alpha,
                                    _num_threads, _filter_list_size, _num_frozen_points);
    }

    IndexWriteParametersBuilder(const IndexWriteParameters &wp)
        : _search_list_size(wp.search_list_size), _max_degree(wp.max_degree),
          _max_occlusion_size(wp.max_occlusion_size), _saturate_graph(wp.saturate_graph), _alpha(wp.alpha),
          _filter_list_size(wp.filter_list_size), _num_frozen_points(wp.num_frozen_points)
    {
    }
    IndexWriteParametersBuilder(const IndexWriteParametersBuilder &) = delete;
    IndexWriteParametersBuilder &operator=(const IndexWriteParametersBuilder &) = delete;

  private:
    uint32_t _search_list_size{};
    uint32_t _max_degree{};
    uint32_t _max_occlusion_size{defaults::MAX_OCCLUSION_SIZE};
    bool _saturate_graph{defaults::SATURATE_GRAPH};
    float _alpha{defaults::ALPHA};
    uint32_t _num_threads{defaults::NUM_THREADS};
    uint32_t _filter_list_size{defaults::FILTER_LIST_SIZE};
    uint32_t _num_frozen_points{defaults::NUM_FROZEN_POINTS_STATIC};
};

/// <summary>
/// Search state
/// </summary>
enum State : uint8_t
{
    Unknown = 0,
    Success = 1,
    Failure = 2,
    FailureTimeout = 3,      // Fail because of timeout
    FailureException = 4,    // Fail because of Exception
    FailureInvalidLabel = 5, // Fail because of invalid label
    StateCount = 6           // The number of state
};

/// <summary>
/// Use this class to pass in searching parameters and pass out searching result
/// </summary>

template <typename LabelT = uint32_t> class IndexSearchContext
{
  public:
    IndexSearchContext(uint32_t time_limit_in_microseconds = 0u, uint32_t io_limit = UINT32_MAX, bool allowLessThanKResults = false)
        : _time_limit_in_microseconds(time_limit_in_microseconds), _io_limit(io_limit), _result_state(State::Unknown), _allowLessThankResults(allowLessThanKResults)
    {
        _use_filter = false;
        _label = (LabelT)0;
        _total_result_returned = 0;
    }

    void SetLabel(LabelT label, bool use_filter)
    {
        _label = label;
        _use_filter = use_filter;
    }

    void SetState(State state)
    {
        _result_state = state;
    }

    void UpdateResultReturned(size_t result_returned)
    {
        _total_result_returned = result_returned;
    }

    size_t GetResultReturned(size_t result_returned)
    {
        return _total_result_returned;
    }

    State GetState() const
    {
        return _result_state;
    }

    LabelT GetLabel() const
    {
        return _label;
    }

    uint32_t GetIOLimit() const
    {
        return _io_limit;
    }

    bool UseFilter() const
    {
        return _use_filter;
    }

    bool IsSuccess() const
    {
        return _result_state == State::Success;
    }

    bool CheckTimeout()
    {
        if (_time_limit_in_microseconds > 0 && _time_limit_in_microseconds < _timer.elapsed())
        {
            SetState(State::FailureTimeout);
            return true;
        }

        return false;
    }

    QueryStats &GetStats()
    {
        return _stats;
    }

    bool GetAllowLessThanKResults()
    {
        return _allowLessThankResults;
    }

  private:
    uint32_t _time_limit_in_microseconds;
    uint32_t _io_limit;
    State _result_state;
    bool _use_filter;
    LabelT _label;
    Timer _timer;
    QueryStats _stats;
    bool _allowLessThankResults;
    size_t _total_result_returned;
};

} // namespace diskann
