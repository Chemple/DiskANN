#pragma once
#include <abstract_filter_store.h>

namespace diskann
{
// This class is responsible for filter actions in index, and should not be used outside.
template <typename label_type> class InMemFilterStore : public AbstractFilterStore<label_type>
{
  public:
    InMemFilterStore(const size_t num_points);
    ~InMemFilterStore() = default;

    // needs some internal lock
    bool detect_common_filters(uint32_t point_id, bool search_invocation,
                               const std::vector<label_type> &incoming_labels) override;

    const std::vector<label_type> &get_labels_by_point(const location_t point_id);
    const tsl::robin_set<label_type> &get_all_label_set();
    // Throws: out of range exception
    void add_label_to_point(const location_t point_id, label_type label);
    // returns internal mapping for given raw_label
    label_type get_converted_label(const std::string &raw_label);

    void update_medoid_by_label(const label_type &label, const uint32_t new_medoid);
    const uint32_t &get_medoid_by_label(const label_type &label);
    bool label_has_medoid(const label_type &label);
    void calculate_best_medoids(const size_t num_points_to_load, const uint32_t num_candidates);

    // TODO: in future we will support multiple universal labels, for now the size will be 1
    void set_universal_labels(const std::vector<std::string> &raw_universal_labels);

    // TODO: will support this in future
    void set_universal_labels_with_category(const std::unordered_map<std ::string, std::string> category_label_map);
    // const label_type get_universal_labels() const;

    // Takes raw label file and generates internal mapping and loads the mapped labels
    size_t load_raw_labels(const std::string &raw_labels_file);

    void save_labels(const std::string &save_path, const size_t total_points);
    void save_medoids(const std::string &save_path);
    void save_label_map(const std::string &save_path);
    void save_universal_label(const std::string &save_path);

  protected:
    // These are for internal use and will be called while search.
    size_t load_labels(const std::string &labels_file);
    size_t load_medoids(const std::string &labels_to_medoid_file);
    void load_label_map(const std::string &labels_map_file);
    void load_universal_labels(const std::string &universal_labels);

  private:
    size_t _num_points;
    std::vector<std::vector<label_type>> _pts_to_labels;
    tsl::robin_set<label_type> _labels;
    std::unordered_map<std::string, label_type> _label_map;

    // medoids
    std::unordered_map<label_type, uint32_t> _label_to_medoid_id;
    std::unordered_map<uint32_t, uint32_t> _medoid_counts; // medoids only happen for filtered index
    // universal label
    bool _use_universal_label = false;
    tsl::robin_set<std::string> _raw_universal_label_set;
    std::set<label_type> _mapped_universal_label_set;
    //  this map is populated while we do the mapping internally. map internal_map -> raw_labels

    // populates pts_to labels and _labels from given label file
    size_t parse_label_file(const std::string &label_file);
    void convert_labels_string_to_int(const std::string &inFileName, const std::string &outFileName,
                                      const std::string &mapFileName, const std::string &unv_label);
};

} // namespace diskann
