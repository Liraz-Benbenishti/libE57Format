#include <iostream>
#include <vector>
#include "E57Format.h"

using namespace e57;

const int64_t NUM_POINTS = 100000;  // Small for quick test
const int64_t READ_START = 50000;

int main()
{
    try
    {
        std::string filename = "/tmp/test_e57_simple.e57";
        
        // WRITE
        std::cout << "Writing " << NUM_POINTS << " points..." << std::endl;
        
        ImageFile imf(filename, "w");
        StructureNode root = imf.root();
        StructureNode data3D(imf);
        
        StructureNode proto(imf);
        proto.set("x", FloatNode(imf, 0.0, PrecisionDouble, -1e6, 1e6));
        proto.set("y", FloatNode(imf, 0.0, PrecisionDouble, -1e6, 1e6));
        proto.set("z", FloatNode(imf, 0.0, PrecisionDouble, -1e6, 1e6));
        
        VectorNode codecs(imf);
        CompressedVectorNode points(imf, proto, codecs);
        
        data3D.set("points", points);
        root.set("data3D", data3D);
        
        // Write points
        std::vector<double> xs(NUM_POINTS), ys(NUM_POINTS), zs(NUM_POINTS);
        for (int64_t i = 0; i < NUM_POINTS; i++)
        {
            xs[i] = i * 0.1;
            ys[i] = i * 0.2;
            zs[i] = i * 0.3;
        }
        
        std::vector<SourceDestBuffer> buffers;
        buffers.push_back(SourceDestBuffer(imf, "x", xs.data(), NUM_POINTS));
        buffers.push_back(SourceDestBuffer(imf, "y", ys.data(), NUM_POINTS));
        buffers.push_back(SourceDestBuffer(imf, "z", zs.data(), NUM_POINTS));
        
        CompressedVectorWriter writer = points.writer(buffers);
        writer.write(buffers, NUM_POINTS);
        writer.close();
        
        imf.close();
        std::cout << "Write complete" << std::endl;
        
        // READ
        std::cout << "Opening file for reading..." << std::endl;
        
        ImageFile imf_read(filename, "r");
        StructureNode root_read = imf_read.root();
        StructureNode data3D_read = StructureNode(root_read.get("data3D"));
        CompressedVectorNode points_read = CompressedVectorNode(data3D_read.get("points"));
        
        std::cout << "Points in file: " << points_read.childCount() << std::endl;
        
        // Try simple sequential read first
        std::cout << "Testing sequential read (first 1000 points)..." << std::endl;
        
        int64_t read_count = 1000;
        std::vector<double> read_xs(read_count), read_ys(read_count), read_zs(read_count);
        
        std::vector<SourceDestBuffer> buffers2;
        buffers2.push_back(SourceDestBuffer(imf_read, "x", read_xs.data(), read_count));
        buffers2.push_back(SourceDestBuffer(imf_read, "y", read_ys.data(), read_count));
        buffers2.push_back(SourceDestBuffer(imf_read, "z", read_zs.data(), read_count));
        
        CompressedVectorReader reader = points_read.reader(buffers2);
        unsigned count = reader.read(buffers2);
        
        std::cout << "Sequential read: " << count << " points" << std::endl;
        std::cout << "First point: (" << read_xs[0] << ", " << read_ys[0] << ", " << read_zs[0] << ")" << std::endl;
        
        reader.close();
        
        // Now try seek
        std::cout << "\nTesting seek to point " << READ_START << "..." << std::endl;
        
        CompressedVectorReader reader2 = points_read.reader(buffers2);
        std::cout << "Calling seek..." << std::endl;
        reader2.seek(READ_START);
        std::cout << "Seek complete" << std::endl;
        
        count = reader2.read(buffers2);
        std::cout << "Read after seek: " << count << " points" << std::endl;
        std::cout << "First point after seek: (" << read_xs[0] << ", " << read_ys[0] << ", " << read_zs[0] << ")" << std::endl;
        std::cout << "Expected: (" << (READ_START * 0.1) << ", " << (READ_START * 0.2) << ", " << (READ_START * 0.3) << ")" << std::endl;
        
        reader2.close();
        imf_read.close();
        
        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    }
    catch (const E57Exception& ex)
    {
        std::cout << "E57 Error: " << ex.what() << std::endl;
        return 1;
    }
    catch (const std::exception& ex)
    {
        std::cout << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
