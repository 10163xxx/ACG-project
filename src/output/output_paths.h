#pragma once

#include <string>

namespace OutputPaths {

    inline const std::string DebugDirectory =
        "results/debug/";

    inline const std::string TestDirectory =
        "results/tests/";

    inline const std::string RenderDirectory =
        "results/renders/";

    inline const std::string CameraTest =
        DebugDirectory + "camera_test.ppm";

    inline const std::string GradientTest =
        DebugDirectory + "gradient_test.ppm";

}
