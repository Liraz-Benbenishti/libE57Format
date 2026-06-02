#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include "E57Format.h"
#include "E57SimpleWriter.h"

using namespace e57;
using namespace std::chrono;

const int64_t NUM_POINTS = 453000000;  // Write 453M points
const int64_t SEEK_POINT = 440000000;   // Seek to 440M point
const int64_t READ_POINTS = 1000000;  // Read 1M points

int main(int argc, char* argv[])
{
    try
    {
        std::string filename = "test_large.e57";
        
        // ============================================================
        // PHASE 1: WRITE E57 FILE WITH 453M POINTS
        // ============================================================
        std::cout << "========================================" << std::endl;
        std::cout << "PHASE 1: Writing 453M point E57 file" << std::endl;
        std::cout << "========================================" << std::endl;
        
        auto write_start = high_resolution_clock::now();
        
        WriterOptions options;
        options.guid = "test_seek_large_guid";
        Writer writer(filename, options);

        Data3D data3DHeader;
        data3DHeader.guid = "test_seek_large_data3D";
        data3DHeader.pointCount = NUM_POINTS;
        data3DHeader.pointFields.cartesianXField = true;
        data3DHeader.pointFields.cartesianYField = true;
        data3DHeader.pointFields.cartesianZField = true;
        data3DHeader.pointFields.intensityField = true;
        data3DHeader.intensityLimits.intensityMinimum = 0.0;
        data3DHeader.intensityLimits.intensityMaximum = 65535.0;
        data3DHeader.cartesianBounds.xMinimum = -20.0;
        data3DHeader.cartesianBounds.xMaximum = 20.0;
        data3DHeader.cartesianBounds.yMinimum = -20.0;
        data3DHeader.cartesianBounds.yMaximum = 20.0;
        data3DHeader.cartesianBounds.zMinimum = -10.0;
        data3DHeader.cartesianBounds.zMaximum = 30.0;

        int64_t dataIndex = writer.NewData3D(data3DHeader);
        ImageFile imf = writer.GetRawIMF();
        VectorNode data3D = writer.GetRawData3D();
        StructureNode scan(data3D.get(dataIndex));
        CompressedVectorNode compPoints(scan.get("points"));

        // Buffer size for batch writing
        const int64_t BATCH_SIZE = 100000;
        
        std::cout << "Writing " << NUM_POINTS << " points in batches of " << BATCH_SIZE << std::endl;
        
        std::vector<double> xs(BATCH_SIZE), ys(BATCH_SIZE), zs(BATCH_SIZE);
        std::vector<double> intensities(BATCH_SIZE);
        std::vector<SourceDestBuffer> srcbufs;
        srcbufs.push_back(SourceDestBuffer(imf, "cartesianX", xs.data(), BATCH_SIZE));
        srcbufs.push_back(SourceDestBuffer(imf, "cartesianY", ys.data(), BATCH_SIZE));
        srcbufs.push_back(SourceDestBuffer(imf, "cartesianZ", zs.data(), BATCH_SIZE));
        srcbufs.push_back(SourceDestBuffer(imf, "intensity", intensities.data(), BATCH_SIZE));
        
        CompressedVectorWriter writerPoints = compPoints.writer(srcbufs);
        
        int64_t total_written = 0;
        int64_t progress_interval = 50000000;  // Print progress every 50M points
        int64_t next_progress = progress_interval;
        
        while (total_written < NUM_POINTS)
        {
            int64_t batch = std::min(BATCH_SIZE, NUM_POINTS - total_written);
            
            // Generate synthetic data
            for (int64_t i = 0; i < batch; i++)
            {
                int64_t point_id = total_written + i;
                double angle = static_cast<double>(point_id) * 0.001;
                double r = 10.0 + 5.0 * std::sin(angle);
                
                xs[i] = r * std::cos(angle);
                ys[i] = r * std::sin(angle);
                zs[i] = 10.0 + 2.0 * std::sin(angle * 2.0);
                intensities[i] = static_cast<double>(point_id % 65536);
            }
            
            writerPoints.write(srcbufs, batch);
            
            total_written += batch;
            
            if (total_written >= next_progress)
            {
                std::cout << "  Written " << total_written << " points..." << std::endl;
                next_progress += progress_interval;
            }
        }
        
        writerPoints.close();
        writer.Close();
        
        auto write_end = high_resolution_clock::now();
        auto write_duration = duration_cast<seconds>(write_end - write_start);
        
        std::cout << "✓ File written successfully in " << write_duration.count() << " seconds" << std::endl;
        std::cout << "  File size: " << NUM_POINTS << " points" << std::endl;
        
        // ============================================================
        // PHASE 2: READ 1M POINTS STARTING FROM 440M (RANDOM ACCESS)
        // ============================================================
        std::cout << "\n========================================" << std::endl;
        std::cout << "PHASE 2: Random access read from 440M" << std::endl;
        std::cout << "========================================" << std::endl;
        
        ImageFile imf_read(filename, "r");
        StructureNode root_read = imf_read.root();
        VectorNode data3D_vector(root_read.get("data3D"));
        StructureNode data3D_read(data3D_vector.get(0));
        CompressedVectorNode compPoints_read = CompressedVectorNode(data3D_read.get("points"));
        
        std::cout << "Total points in file: " << compPoints_read.childCount() << std::endl;
        std::cout << "Seeking to point: " << SEEK_POINT << std::endl;
        std::cout << "Reading: " << READ_POINTS << " points" << std::endl;
        
        // Prepare buffers for reading
        std::vector<double> read_xs(READ_POINTS), read_ys(READ_POINTS), read_zs(READ_POINTS);
        std::vector<double> read_intensities(READ_POINTS);
        
        std::vector<SourceDestBuffer> dbufs;
        dbufs.push_back(SourceDestBuffer(imf_read, "cartesianX", read_xs.data(), READ_POINTS));
        dbufs.push_back(SourceDestBuffer(imf_read, "cartesianY", read_ys.data(), READ_POINTS));
        dbufs.push_back(SourceDestBuffer(imf_read, "cartesianZ", read_zs.data(), READ_POINTS));
        dbufs.push_back(SourceDestBuffer(imf_read, "intensity", read_intensities.data(), READ_POINTS));
        
        CompressedVectorReader reader = compPoints_read.reader(dbufs);
        
        // Measure seek and read time
        auto seek_start = high_resolution_clock::now();
        reader.seek(SEEK_POINT);
        auto seek_end = high_resolution_clock::now();
        
        auto seek_duration = duration_cast<milliseconds>(seek_end - seek_start);
        std::cout << "✓ Seek time: " << seek_duration.count() << " ms" << std::endl;
        
        auto read_start = high_resolution_clock::now();
        unsigned read_count = reader.read(dbufs);
        auto read_end = high_resolution_clock::now();
        
        auto read_duration = duration_cast<milliseconds>(read_end - read_start);
        std::cout << "✓ Read time: " << read_duration.count() << " ms" << std::endl;
        std::cout << "  Points read: " << read_count << std::endl;
        
        reader.close();
        imf_read.close();
        
        // ============================================================
        // PHASE 3: VALIDATE DATA
        // ============================================================
        std::cout << "\n========================================" << std::endl;
        std::cout << "PHASE 3: Validating read data" << std::endl;
        std::cout << "========================================" << std::endl;
        
        if (read_count != READ_POINTS)
        {
            std::cout << "✗ ERROR: Expected " << READ_POINTS << " points but got " << read_count << std::endl;
            return 1;
        }
        
        // Validate that the points correspond to indices 440M-441M
        int64_t errors = 0;
        int64_t check_count = (READ_POINTS < 1000LL) ? READ_POINTS : 1000LL;
        for (int64_t i = 0; i < check_count; i++)  // Check first 1000 points
        {
            int64_t point_id = SEEK_POINT + i;
            double angle = static_cast<double>(point_id) * 0.001;
            double r = 10.0 + 5.0 * std::sin(angle);
            
            double expected_x = r * std::cos(angle);
            double expected_y = r * std::sin(angle);
            double expected_z = 10.0 + 2.0 * std::sin(angle * 2.0);
            double expected_intensity = static_cast<double>(point_id % 65536);
            
            double tolerance = 1e-6;
            
            if (std::abs(read_xs[i] - expected_x) > tolerance ||
                std::abs(read_ys[i] - expected_y) > tolerance ||
                std::abs(read_zs[i] - expected_z) > tolerance ||
                std::abs(read_intensities[i] - expected_intensity) > tolerance)
            {
                errors++;
                if (errors <= 5)  // Print first 5 errors
                {
                    std::cout << "✗ Mismatch at point " << i << " (id: " << point_id << ")" << std::endl;
                    std::cout << "  Expected: x=" << expected_x << " y=" << expected_y 
                              << " z=" << expected_z << " intensity=" << expected_intensity << std::endl;
                    std::cout << "  Got:      x=" << read_xs[i] << " y=" << read_ys[i] 
                              << " z=" << read_zs[i] << " intensity=" << read_intensities[i] << std::endl;
                }
            }
        }
        
        if (errors == 0)
        {
            std::cout << "✓ All validated points match expected values!" << std::endl;
        }
        else
        {
            std::cout << "✗ Found " << errors << " validation errors in first 1000 points" << std::endl;
            return 1;
        }
        
        // ============================================================
        // FINAL SUMMARY
        // ============================================================
        std::cout << "\n========================================" << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "✓ Write time:   " << write_duration.count() << " seconds" << std::endl;
        std::cout << "✓ Seek time:    " << seek_duration.count() << " ms" << std::endl;
        std::cout << "✓ Read time:    " << read_duration.count() << " ms" << std::endl;
        std::cout << "✓ Read rate:    " << (READ_POINTS / (read_duration.count() / 1000.0) / 1e6) 
                  << " million points/sec" << std::endl;
        std::cout << "✓ Data validation: PASSED" << std::endl;
        
        return 0;
    }
    catch (const E57Exception& ex)
    {
        std::cout << "E57 Error: " << ex.errorStr() << " (code=" << ex.errorCode() << ")" << std::endl;
        ex.report("test_seek_large.cpp", 0, "main", std::cout);
        return 1;
    }
    catch (const std::exception& ex)
    {
        std::cout << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
