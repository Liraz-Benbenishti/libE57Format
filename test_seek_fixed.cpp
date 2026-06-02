#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include "E57Format.h"

using namespace e57;
using namespace std::chrono;

// Use smaller numbers for now, but structure supports full 453M
const int64_t NUM_POINTS = 10000000;  // 10M for testing
const int64_t SEEK_POINT = 5000000;   // Seek to 5M   
const int64_t READ_POINTS = 1000000;  // Read 1M points

int main()
{
    try
    {
        std::string filename = "/tmp/test_e57_seek.e57";
        
        std::cout << "========================================" << std::endl;
        std::cout << "E57 Random Access Seek Test" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Points to write: " << NUM_POINTS << std::endl;
        std::cout << "Seek position: " << SEEK_POINT << std::endl;
        std::cout << "Points to read: " << READ_POINTS << std::endl;
        std::cout << std::endl;
        
        // ============================================================
        // PHASE 1: WRITE E57 FILE
        // ============================================================
        std::cout << "PHASE 1: Writing E57 file with " << NUM_POINTS << " points..." << std::endl;
        
        auto write_start = high_resolution_clock::now();
        
        ImageFile imf(filename, "w");
        StructureNode root = imf.root();
        
        // Create data3D vector of scans
        VectorNode data3D(imf);
        StructureNode scan(imf);
        
        // Create point cloud compressed vector
        StructureNode proto(imf);
        proto.set("x", FloatNode(imf, 0.0, PrecisionDouble, -1e6, 1e6));
        proto.set("y", FloatNode(imf, 0.0, PrecisionDouble, -1e6, 1e6));
        proto.set("z", FloatNode(imf, 0.0, PrecisionDouble, -1e6, 1e6));
        proto.set("intensity", IntegerNode(imf, 0, 0, 65535));
        
        VectorNode codecs(imf);
        CompressedVectorNode points(imf, proto, codecs);
        
        scan.set("points", points);
        data3D.append(scan);
        root.set("data3D", data3D);
        
        // Write points in batches
        const int64_t BATCH_SIZE = 100000;
        std::vector<double> xs(BATCH_SIZE), ys(BATCH_SIZE), zs(BATCH_SIZE);
        std::vector<int64_t> intensities(BATCH_SIZE);
        std::vector<SourceDestBuffer> buffers;
        buffers.push_back(SourceDestBuffer(imf, "x", xs.data(), BATCH_SIZE));
        buffers.push_back(SourceDestBuffer(imf, "y", ys.data(), BATCH_SIZE));
        buffers.push_back(SourceDestBuffer(imf, "z", zs.data(), BATCH_SIZE));
        buffers.push_back(SourceDestBuffer(imf, "intensity", intensities.data(), BATCH_SIZE));
        
        CompressedVectorWriter writer = points.writer(buffers);
        int64_t written = 0;
        
        while (written < NUM_POINTS)
        {
            int64_t batch = std::min(BATCH_SIZE, NUM_POINTS - written);
            
            // Generate synthetic points
            for (int64_t i = 0; i < batch; i++)
            {
                int64_t pid = written + i;
                double angle = pid * 0.0001;
                double r = 100.0 + 50.0 * std::sin(angle);
                
                xs[i] = r * std::cos(angle);
                ys[i] = r * std::sin(angle);
                zs[i] = 50.0 + 20.0 * std::sin(angle * 2);
                intensities[i] = pid % 65536;
            }
            
            writer.write(buffers, batch);
            
            written += batch;
            
            if (written % 1000000 == 0)
            {
                std::cout << "  Wrote " << written << " points..." << std::endl;
            }
        }
        
        writer.close();
        
        imf.close();
        
        auto write_end = high_resolution_clock::now();
        auto write_duration = duration_cast<seconds>(write_end - write_start);
        
        std::cout << "✓ File written in " << write_duration.count() << " seconds" << std::endl;
        std::cout << std::endl;
        
        // ============================================================
        // PHASE 2: TEST RANDOM ACCESS SEEK
        // ============================================================
        std::cout << "PHASE 2: Testing random access from point " << SEEK_POINT << "..." << std::endl;
        
        ImageFile imf_read(filename, "r");
        std::cout << "Opened file for read." << std::endl;
        StructureNode root_read = imf_read.root();
        std::cout << "Got root node." << std::endl;
        StructureNode data3D_read = StructureNode(root_read.get("data3D"));
        std::cout << "Got data3D node." << std::endl;
        CompressedVectorNode points_read = CompressedVectorNode(data3D_read.get("points"));
        std::cout << "Got points node." << std::endl;
        
        std::cout << "Total points in file: " << points_read.childCount() << std::endl;
        std::cout << "Creating reader..." << std::endl;
        
        // Prepare read buffers
        std::vector<double> read_xs(READ_POINTS), read_ys(READ_POINTS), read_zs(READ_POINTS);
        std::vector<int64_t> read_intensities(READ_POINTS);
        
        std::vector<SourceDestBuffer> dbufs;
        dbufs.push_back(SourceDestBuffer(imf_read, "x", read_xs.data(), READ_POINTS));
        dbufs.push_back(SourceDestBuffer(imf_read, "y", read_ys.data(), READ_POINTS));
        dbufs.push_back(SourceDestBuffer(imf_read, "z", read_zs.data(), READ_POINTS));
        dbufs.push_back(SourceDestBuffer(imf_read, "intensity", read_intensities.data(), READ_POINTS));
        
        CompressedVectorReader reader = points_read.reader(dbufs);
        std::cout << "Created reader." << std::endl;
        
        // Measure seek time
        auto seek_start = high_resolution_clock::now();
        std::cout << "Calling seek..." << std::endl;
        reader.seek(SEEK_POINT);
        std::cout << "Seek returned." << std::endl;
        auto seek_end = high_resolution_clock::now();
        auto seek_duration = duration_cast<microseconds>(seek_end - seek_start);
        
        std::cout << "✓ Seek time: " << seek_duration.count() / 1000.0 << " ms" << std::endl;
        
        // Measure read time
        auto read_start = high_resolution_clock::now();
        unsigned read_count = reader.read(dbufs);
        auto read_end = high_resolution_clock::now();
        auto read_duration = duration_cast<milliseconds>(read_end - read_start);
        
        std::cout << "✓ Read time: " << read_duration.count() << " ms" << std::endl;
        std::cout << "  Points read: " << read_count << std::endl;
        
        if (read_count != READ_POINTS)
        {
            std::cout << "✗ ERROR: Expected " << READ_POINTS << " but got " << read_count << std::endl;
            return 1;
        }
        
        reader.close();
        imf_read.close();
        
        // ============================================================
        // PHASE 3: VALIDATE DATA
        // ============================================================
        std::cout << std::endl;
        std::cout << "PHASE 3: Validating data integrity..." << std::endl;
        
        int64_t errors = 0;
        int64_t check_count = (READ_POINTS < 1000) ? READ_POINTS : 1000;
        
        for (int64_t i = 0; i < check_count; i++)
        {
            int64_t pid = SEEK_POINT + i;
            double angle = pid * 0.0001;
            double r = 100.0 + 50.0 * std::sin(angle);
            
            double exp_x = r * std::cos(angle);
            double exp_y = r * std::sin(angle);
            double exp_z = 50.0 + 20.0 * std::sin(angle * 2);
            int64_t exp_intensity = pid % 65536;
            
            double tol = 1e-10;
            if (std::abs(read_xs[i] - exp_x) > tol ||
                std::abs(read_ys[i] - exp_y) > tol ||
                std::abs(read_zs[i] - exp_z) > tol ||
                read_intensities[i] != exp_intensity)
            {
                errors++;
                if (errors <= 3)
                {
                    std::cout << "✗ Mismatch at index " << i << " (point ID " << pid << ")" << std::endl;
                    std::cout << "  Expected: (" << exp_x << ", " << exp_y << ", " << exp_z 
                              << ") intensity=" << exp_intensity << std::endl;
                    std::cout << "  Got:      (" << read_xs[i] << ", " << read_ys[i] << ", " 
                              << read_zs[i] << ") intensity=" << read_intensities[i] << std::endl;
                }
            }
        }
        
        if (errors == 0)
        {
            std::cout << "✓ All " << check_count << " validated points match expected values!" << std::endl;
        }
        else
        {
            std::cout << "✗ Found " << errors << " validation errors" << std::endl;
            return 1;
        }
        
        // ============================================================
        // SUMMARY
        // ============================================================
        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "TEST RESULTS" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "✓ Write:          " << write_duration.count() << " seconds" << std::endl;
        std::cout << "✓ Seek:           " << seek_duration.count() / 1000.0 << " ms" << std::endl;
        std::cout << "✓ Read 1M points: " << read_duration.count() << " ms" << std::endl;
        std::cout << "✓ Read rate:      " << (READ_POINTS * 1000.0 / read_duration.count() / 1e6) 
                  << " million points/sec" << std::endl;
        std::cout << "✓ Data validation: PASSED" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    }
    catch (const E57Exception& ex)
    {
        std::cout << "E57 Error: " << ex.errorStr() << " (code=" << ex.errorCode() << ")" << std::endl;
        ex.report("test_seek_fixed.cpp", 0, "main", std::cout);
        return 1;
    }
    catch (const std::exception& ex)
    {
        std::cout << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
