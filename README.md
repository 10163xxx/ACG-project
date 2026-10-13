# ACG Path Tracer — 项目开发记录

## 1. 项目简介

本项目是 Advanced Computer Graphics 2026 Topic 1: Image Rendering 的实现项目，采用 C++17 和 CMake 构建。

项目以路径追踪（Path Tracing）为核心目标，逐步实现几何求交、材质散射、光线传输、加速结构和高级渲染功能。

当前阶段的重点是建立稳定、清晰、可扩展的代码架构，而不是直接实现完整的最终渲染效果。

## 2. 当前项目结构

```text
ACG-PathTracer/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── vec3.h
│   │   ├── ray.h
│   │   ├── interval.h
│   │   └── random.h
│   ├── geometry/
│   │   ├── hit_record.h
│   │   ├── hittable.h
│   │   ├── sphere.h
│   │   ├── triangle.h
│   │   ├── hittable_list.h
│   │   ├── aabb.h
│   │   └── mesh.h
│   ├── camera/
│   │   └── camera.h
│   ├── materials/
│   │   ├── material.h
│   │   ├── lambertian.h
│   │   ├── metal.h
│   │   └── dielectric.h
│   ├── sampling/
│   │   ├── sampling.h
│   │   └── pdf.h
│   ├── acceleration/
│   │   └── bvh.h
│   ├── integrator/
│   │   └── path_tracer.h
│   ├── scene/
│   │   └── test_scene.h
│   └── output/
│       ├── image.h
│       └── output_paths.h
├── tests/
│   ├── test_geometry.cpp
│   ├── test_materials.cpp
│   └── test_bvh.cpp
├── results/
│   ├── debug/
│   ├── tests/
│   └── renders/
├── assets/
└── build/
```

**注意：** 上述目录树包含规划中的模块。目录或文件存在不代表其功能已经实现。当前应以实际源代码为准。

## 3. 核心模块与接口

### 3.1 `core/vec3.h` — 三维向量

定义 `Vec3`，用于表示三维位置、方向、法线和 RGB 颜色。

当前接口：

```cpp
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_);

    Vec3 operator+(const Vec3& v) const;
    Vec3 operator-(const Vec3& v) const;
    Vec3 operator*(double s) const;
    Vec3 operator/(double s) const;

    double length_squared() const;
    double length() const;
    bool near_zero() const;
};

Vec3 operator*(double s, const Vec3& v);

double dot(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);
Vec3 normalize(const Vec3& v);
```

说明：

- `dot()`：计算点积，用于角度、法线和方向判断。
- `cross()`：计算叉积，用于构造垂直方向。
- `normalize()`：将向量归一化。
- `near_zero()`：判断向量是否接近零。

当前版本尚未包含所有后续渲染所需的向量运算，例如分量乘法、随机方向生成等，后续根据材质和积分器的需求逐步扩展。

### 3.2 `core/ray.h` — 射线

定义 `Ray`，表示从原点沿某一方向传播的射线。

```cpp
struct Ray {
    Vec3 origin;
    Vec3 direction;

    Ray() = default;
    Ray(const Vec3& origin_, const Vec3& direction_);

    Vec3 at(double t) const;
};
```

数学定义：

\[
P(t)=O+tD
\]

其中 \(O\) 是射线原点，\(D\) 是方向，\(t\) 是参数。

`at(t)` 返回射线上参数为 \(t\) 的位置。

### 3.3 `core/interval.h` — 参数区间

定义 `Interval`，用于限制射线求交的有效参数范围。

```cpp
struct Interval {
    double min;
    double max;

    Interval();
    Interval(double min_, double max_);

    bool contains(double t) const;
    bool surrounds(double t) const;
    bool is_empty() const;
};
```

- `contains(t)`：判断 \(t\) 是否处于闭区间内。
- `surrounds(t)`：判断 \(t\) 是否严格处于区间内部。
- `is_empty()`：判断区间是否为空。

求交时通常使用严格区间，避免接受射线原点附近或区间边界上的不期望交点。

### 3.4 `geometry/hit_record.h` — 求交记录

`HitRecord` 保存射线与几何体相交后的信息。

```cpp
class Material;

struct HitRecord {
    Vec3 point;
    Vec3 normal;
    double t = 0.0;
    bool front_face = true;

    std::shared_ptr<const Material> material;

    void set_face_normal(
        const Ray& ray,
        const Vec3& outward_normal
    );
};
```

字段含义：

- `point`：交点位置。
- `normal`：朝向入射射线一侧的法线。
- `t`：交点对应的射线参数。
- `front_face`：射线是否从物体外部击中表面。
- `material`：交点对应的材质共享指针。

`set_face_normal()` 根据入射方向与外部法线的点积统一法线朝向。

材质使用前向声明和 `std::shared_ptr<const Material>`，避免 `hit_record.h` 与具体材质实现形成循环依赖。

### 3.5 `geometry/hittable.h` — 几何体抽象接口

所有可被射线求交的物体都应实现 `Hittable`。

```cpp
class Hittable {
public:
    virtual ~Hittable() = default;

    virtual bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const = 0;
};
```

这是几何模块的统一接口：

- 输入射线和有效参数区间。
- 求交成功时将信息写入 `HitRecord`。
- 返回是否发生有效交点。

后续的球体、三角形、网格和 BVH 都可以通过该接口参与场景求交。

### 3.6 `geometry/sphere.h` — 球体

`Sphere` 是当前已实现的具体几何体，继承自 `Hittable`。

```cpp
class Sphere : public Hittable {
public:
    Sphere(
        const Vec3& center,
        double radius,
        std::shared_ptr<const Material> material = nullptr
    );

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override;

private:
    Vec3 center_;
    double radius_;
    std::shared_ptr<const Material> material_;
};
```

构造参数：

- `center`：球心。
- `radius`：半径，必须大于零。
- `material`：材质指针，默认允许为空，以保持现有诊断程序的兼容性。

`hit()` 使用球体的二次方程计算最近有效交点，设置交点位置、法线、正反面信息，并将球体材质写入 `HitRecord`。

当材质为空时，几何求交仍然可以成功；但后续路径追踪器必须明确处理无材质的情况。

### 3.7 `geometry/hittable_list.h` — 几何体集合

`HittableList` 继承自 `Hittable`，通过遍历内部对象寻找最近交点。

主要接口：

```cpp
class HittableList : public Hittable {
public:
    HittableList() = default;

    void clear();

    void add(std::shared_ptr<const Hittable> object);

    std::size_t size() const;

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override;
};
```

内部使用：

```cpp
std::vector<std::shared_ptr<const Hittable>> objects_;
```

`add()` 不允许添加空对象。求交时，集合逐个检查对象，并不断缩小有效区间上限，最终返回最近的有效交点。

当前实现采用线性遍历。后续可以通过 BVH 加速大规模场景的求交。

### 3.8 `camera/camera.h` — 相机

`Camera` 根据观察位置、目标点、上方向、垂直视场角和画面宽高比构造相机坐标系。

```cpp
class Camera {
public:
    Camera(
        const Vec3& look_from,
        const Vec3& look_at,
        const Vec3& vup,
        double vertical_fov_degrees,
        double aspect_ratio
    );

    Ray get_ray(double s, double t) const;
};
```

- `look_from`：相机位置。
- `look_at`：观察目标。
- `vup`：参考上方向。
- `vertical_fov_degrees`：垂直视场角，单位为度。
- `aspect_ratio`：图像宽高比。
- `get_ray(s, t)`：生成穿过图像平面归一化坐标位置的射线。

构造函数会检查宽高比、视场角以及相机观察方向是否有效。

当前相机用于基础针孔相机渲染，景深和运动模糊尚待后续扩展。

### 3.9 `materials/material.h` — 材质抽象接口

材质模块定义表面如何与入射射线相互作用。

```cpp
class Material {
public:
    virtual ~Material() = default;

    virtual bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered
    ) const = 0;
};
```

接口参数：

- `incoming`：入射射线。
- `hit`：表面交点记录。
- `attenuation`：本次散射的颜色衰减或路径吞吐量乘数。
- `scattered`：散射后生成的射线。
- 返回值：是否产生有效散射。

该接口目前只定义了抽象行为。Lambertian、Metal 和 Dielectric 的具体散射算法仍需实现。

后续若支持发光材质、Principled BSDF、重要性采样和多种光传输策略，材质接口可能进一步扩展。

### 3.10 `output/image.h` — 图像输出

`Image` 负责保存像素颜色并将图像写入 PPM 文件。

主要操作：

```cpp
class Image {
public:
    Image(int width, int height);

    void set_pixel(int x, int y, const Vec3& color);
    Vec3 get_pixel(int x, int y) const;

    void write_ppm(const std::string& filename) const;
};
```

当前实现使用 `Vec3` 保存像素颜色，采用 P3 ASCII 格式写出 PPM 图像。写文件前会将颜色分量限制在 `[0, 1]`，再转换为 `[0, 255]` 的整数值；如果输出路径包含尚不存在的父目录，会尝试创建目录。

当前输出没有实现完整的色调映射、HDR 输出或色彩管理流程。

### 3.11 `output/output_paths.h` — 输出路径

集中保存诊断图片、测试结果和正式渲染的默认路径：

```cpp
namespace OutputPaths {
    inline const std::string DebugDirectory = "results/debug/";
    inline const std::string TestDirectory = "results/tests/";
    inline const std::string RenderDirectory = "results/renders/";

    inline const std::string CameraTest =
        DebugDirectory + "camera_test.ppm";

    inline const std::string GradientTest =
        DebugDirectory + "gradient_test.ppm";
}
```

这些路径用于区分开发诊断输出、自动测试输出和正式渲染结果。

## 4. 当前模块依赖关系

当前主要调用关系如下：

```text
Camera
  └── Ray
        └── Hittable::hit()
              ├── Sphere
              └── HittableList
                    └── HitRecord
                          └── Material

Image
  └── 保存并输出最终像素颜色
```

其中，`HitRecord` 是几何求交与材质系统之间的桥梁。

几何体负责确定交点；材质负责描述表面相互作用；未来的 `PathTracer` 将负责调度射线、累计光能并确定最终像素颜色。

## 5. 当前程序行为

当前 `main.cpp` 仍然是基础诊断渲染程序，而不是完整路径追踪器。

其主要流程为：

1. 创建图像。
2. 初始化相机。
3. 创建球体并加入 `HittableList`。
4. 遍历像素，为每个像素生成相机射线。
5. 执行几何求交。
6. 根据表面法线或背景渐变计算诊断颜色。
7. 将图像写入 PPM 文件。

该程序用于验证相机、射线、球体求交和图像输出等基础模块。

虽然目前已经建立材质抽象接口，但诊断程序尚未使用完整的材质散射流程，因此不能将当前结果视为完整的路径追踪渲染结果。

## 6. 构建与运行

项目使用 CMake 和 C++17。

### 配置项目

```powershell
cmake -S . -B build
```

### 编译

```powershell
cmake --build build --config Release
```

### 运行

如果使用 Visual Studio 多配置生成器：

```powershell
.\build\Release\path_tracer.exe
```

如果使用单配置生成器：

```powershell
.\build\path_tracer.exe
```

具体可执行文件路径取决于所使用的 CMake 生成器。

## 7. 测试策略

项目计划将正式渲染程序与独立测试程序分开，避免每增加一个功能就需要反复改写 `main.cpp`。

规划中的测试目标：

- `test_geometry`：验证射线与几何体的求交。
- `test_materials`：验证材质散射行为。
- `test_bvh`：验证加速结构的求交正确性。
- `path_tracer`：运行正式渲染流程。

上述测试目标仍需在 CMake 中逐步配置，并补充对应的测试代码。

在独立测试目标尚未建立之前，现有 `main.cpp` 继续承担基础图像诊断任务。

## 8. 下一阶段开发计划

### 阶段一：基础材质

1. 完善随机数生成与向量采样工具。
2. 实现 Lambertian 漫反射。
3. 实现 Metal 镜面反射。
4. 实现 Dielectric 折射、反射和全反射。
5. 为材质建立独立测试。

### 阶段二：路径追踪积分器

1. 创建 `PathTracer`。
2. 实现材质散射与多次反弹。
3. 实现背景环境光。
4. 累计路径吞吐量与最终辐射亮度。
5. 验证不同材质的渲染结果。

### 阶段三：几何与加速

1. 完善三角形求交。
2. 实现网格对象。
3. 实现 AABB 包围盒。
4. 实现 BVH 加速结构。
5. 验证 BVH 与线性遍历的一致性。

### 阶段四：高级渲染

在基础路径追踪器稳定后，逐步实现：

- 点光源与面光源。
- 重要性采样、PDF 与 MIS。
- Russian Roulette 路径终止。
- 纹理、法线贴图和高度贴图。
- 景深、运动模糊和抗锯齿。
- 透明、半透明与体积效果。
- Principled BSDF、多层材质和更高级的外观模型。
- 环境光照、色调映射及其他高级渲染功能。

这些功能属于后续目标，并非当前已完成的功能。

## 9. 当前开发原则

1. **先建立稳定接口，再实现复杂算法。**
2. 几何求交、材质散射、光传输积分和图像输出保持职责分离。
3. 新功能应有独立、可重复的验证方式。
4. 尽可能通过新增模块和测试目标扩展系统，而不是持续堆叠到 `main.cpp` 中。
5. 每个阶段完成后先验证编译和基础行为，再进入下一阶段。
