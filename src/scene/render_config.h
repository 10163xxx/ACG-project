#pragma once

#include <memory>
#include <string>
#include <vector>

#include "camera/camera.h"
#include "geometry/hittable_list.h"
#include "materials/point_light.h"

enum class SceneKind { Showcase, Greenhouse, Test };
enum class TestKind {
    UV, Volume, InhomogeneousVolume, Mipmap, MipmapNoFilter, OBJ, Materials
};

struct SceneRequest {
    SceneKind scene = SceneKind::Showcase;
    TestKind test = TestKind::UV;
    std::string obj_path;
};

struct RenderSetup {
    HittableList geometry;
    std::shared_ptr<Camera> camera;
    int width = 480;
    int height = 270;
    int samples = 256;
    int max_depth = 24;
    std::string output;
    std::vector<PointLight> point_lights;
    bool use_environment = true;
};
