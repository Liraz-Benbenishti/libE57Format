#include <iostream>
#include <vector>
#include <cmath>
#include "E57Format.h"

using namespace e57;

int main()
{
    try
    {
        const std::string filename = "/workspace/test_debug_e57.e57";

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

        const int64_t N = 10000;
        std::vector<double> xs(N), ys(N), zs(N);
        for (int64_t i = 0; i < N; ++i)
        {
            xs[i] = i * 0.1;
            ys[i] = i * 0.2;
            zs[i] = i * 0.3;
        }

        std::vector<SourceDestBuffer> wbufs;
        wbufs.emplace_back(imf, "x", xs.data(), N);
        wbufs.emplace_back(imf, "y", ys.data(), N);
        wbufs.emplace_back(imf, "z", zs.data(), N);

        CompressedVectorWriter writer = points.writer(wbufs);
        writer.write(wbufs, N);
        writer.close();
        imf.close();

        ImageFile imf2(filename, "r");
        StructureNode root2 = imf2.root();
        StructureNode data3D_2 = StructureNode(root2.get("data3D"));
        CompressedVectorNode points2 = CompressedVectorNode(data3D_2.get("points"));

        const int64_t seekPos = 5000;
        const int64_t readCount = 100;
        std::vector<double> rx(readCount), ry(readCount), rz(readCount);
        std::vector<SourceDestBuffer> rbufs;
        rbufs.emplace_back(imf2, "x", rx.data(), readCount);
        rbufs.emplace_back(imf2, "y", ry.data(), readCount);
        rbufs.emplace_back(imf2, "z", rz.data(), readCount);

        CompressedVectorReader reader = points2.reader(rbufs);
        unsigned count = reader.read(rbufs);
        std::cout << "sequential count=" << count << " first=(" << rx[0] << "," << ry[0] << "," << rz[0] << ")\n";
        reader.close();

        CompressedVectorReader reader2 = points2.reader(rbufs);
        reader2.seek(seekPos);
        count = reader2.read(rbufs);
        std::cout << "seek count=" << count << " first=(" << rx[0] << "," << ry[0] << "," << rz[0] << ")\n";
        std::cout << "expected=(" << (seekPos * 0.1) << "," << (seekPos * 0.2) << "," << (seekPos * 0.3) << ")\n";
        for (int i = 0; i < 5 && i < static_cast<int>(count); ++i)
        {
            std::cout << "  [" << i << "] read=(" << rx[i] << "," << ry[i] << "," << rz[i] << ")";
            std::cout << " expected=(" << ((seekPos + i) * 0.1) << "," << ((seekPos + i) * 0.2) << "," << ((seekPos + i) * 0.3) << ")\n";
        }
        reader2.close();
        imf2.close();

        return 0;
    }
    catch (const E57Exception &ex)
    {
        std::cout << "E57Exception: " << ex.errorStr() << " code=" << ex.errorCode() << "\n";
        ex.report("test_seek_debug_run.cpp", 0, "main", std::cout);
        return 1;
    }
    catch (const std::exception &ex)
    {
        std::cout << "std::exception: " << ex.what() << "\n";
        return 1;
    }
}
