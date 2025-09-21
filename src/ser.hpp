#include <string>
#include <cstdint>
#include <filesystem>


struct SerHeader {
    std::string name;
    int32_t color;
    int32_t endianess;
    int32_t width;
    int32_t height;
    int32_t pixel_depth;
    int32_t frame_count;

    void decode_to_dir(const char *input, const char *output);
};

class SerFile {
public:
    static void decode_to_dir(const std::filesystem::path& input_path, const std::filesystem::path& output_dir);
};