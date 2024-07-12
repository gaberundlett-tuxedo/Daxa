#define DAXA_SHADERLANG DAXA_SHADERLANG_GLSL
#define APPNAME "Daxa Sample: MPM MLS"
#include <0_common/base_app.hpp>

using namespace daxa::types;
#include "shaders/shared.inl"

#include <cstdlib>
#include <time.h>
#include "camera.h"

// Función para generar un valor aleatorio en un rango [min, max]
float random_in_range(float min, float max) {
    return min + static_cast<float>(rand()) / static_cast<float>(RAND_MAX / (max - min));
}

daxa_f32mat3x3 make_identity()
{
    return daxa_f32mat3x3{
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
}

daxa_f32mat3x3 make_zero()
{
    return daxa_f32mat3x3{};
}

daxa_f32mat4x4 glm_mat4_to_daxa_f32mat4x4(glm::mat4 const & mat)
{
    return daxa_f32mat4x4{
      {mat[0][0], mat[0][1], mat[0][2], mat[0][3]},
      {mat[1][0], mat[1][1], mat[1][2], mat[1][3]},
      {mat[2][0], mat[2][1], mat[2][2], mat[2][3]},
      {mat[3][0], mat[3][1], mat[3][2], mat[3][3]},
  };
}

// support for addition, subtraction, multiplication, division
inline daxa_f32vec3 operator+(daxa_f32vec3 a, daxa_f32vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline daxa_f32vec3 operator-(daxa_f32vec3 a, daxa_f32vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline daxa_f32vec3 operator*(daxa_f32vec3 a, daxa_f32vec3 b)
{
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

inline daxa_f32vec3 operator/(daxa_f32vec3 a, daxa_f32vec3 b)
{
    return {a.x / b.x, a.y / b.y, a.z / b.z};
}

// TODO: add more operators here

struct App : BaseApp<App>
{
    bool my_toggle = true;
    camera cam = {};
    daxa::TlasId tlas = {};
    daxa::BlasId blas = {};
    daxa::TaskBlas task_blas{{.initial_blas = {.blas = std::array{blas}}, .name = "blas_task"}};
    daxa::TaskTlas task_tlas{{.initial_tlas = {.tlas = std::array{tlas}}, .name = "tlas_task"}};
    const daxa_u32 ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT = 256; // NOTE: Requested by the spec
    void update_virtual_shader()
    {
        if (my_toggle)
        {
            pipeline_manager.add_virtual_file({
                .name = "custom file!!",
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
                .contents = R"(
                    #pragma once
                    #define MY_TOGGLE 1
                )",
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
                .contents = R"(static const bool MY_TOGGLE = true;)",
#endif
            });
        }
        else
        {
            pipeline_manager.add_virtual_file({
                .name = "custom file!!",
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
                .contents = R"(
                    #pragma once
                    #define MY_TOGGLE 0
                )",
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
                .contents = R"(static const bool MY_TOGGLE = false;)",
#endif
            });
        }
    }

    // clang-format off
    std::shared_ptr<daxa::ComputePipeline> p2g_compute_pipeline = [this]() {
        update_virtual_shader();
        return pipeline_manager.add_compute_pipeline({
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .shader_info = {
                .source = daxa::ShaderFile{"compute.glsl"}, 
                .compile_options = {
                    .defines =  std::vector{daxa::ShaderDefine{"P2G_COMPUTE_FLAG", "1"}},
                }
            },
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .shader_info = {.source = daxa::ShaderFile{"compute.slang"}, .compile_options = {.entry_point = "entry_MPM_P2G"}},
#endif
            .push_constant_size = sizeof(ComputePush),
            .name = "p2g_compute_pipeline",
        }).value();
    }();
    // clang-format on

    // clang-format off
    std::shared_ptr<daxa::ComputePipeline> grid_compute_pipeline = [this]() {
        update_virtual_shader();
        return pipeline_manager.add_compute_pipeline({
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .shader_info = {
                .source = daxa::ShaderFile{"compute.glsl"}, 
                .compile_options = {
                    .defines =  std::vector{daxa::ShaderDefine{"GRID_COMPUTE_FLAG", "1"}},
                }
            },
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .shader_info = {.source = daxa::ShaderFile{"compute.slang"}, .compile_options = {.entry_point = "entry_MPM_grid"}},
#endif
            .push_constant_size = sizeof(ComputePush),
            .name = "grid_compute_pipeline",
        }).value();
    }();
    // clang-format on
    
    // clang-format off
    std::shared_ptr<daxa::ComputePipeline> g2p_compute_pipeline = [this]() {
        update_virtual_shader();
        return pipeline_manager.add_compute_pipeline({
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .shader_info = {
                .source = daxa::ShaderFile{"compute.glsl"}, 
                .compile_options = {
                    .defines =  std::vector{daxa::ShaderDefine{"G2P_COMPUTE_FLAG", "1"}},
                }
            },
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .shader_info = {.source = daxa::ShaderFile{"compute.slang"}, .compile_options = {.entry_point = "entry_MPM_G2P"}},
#endif
            .push_constant_size = sizeof(ComputePush),
            .name = "g2p_compute_pipeline",
        }).value();
    }();
    // clang-format on

    // clang-format off
    std::shared_ptr<daxa::ComputePipeline> sphere_tracing_compute_pipeline = [this]() {
        update_virtual_shader();
        return pipeline_manager.add_compute_pipeline({
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .shader_info = {
                .source = daxa::ShaderFile{"compute.glsl"}, 
                .compile_options = {
                    .defines =  std::vector{daxa::ShaderDefine{"SPHERE_TRACING_FLAG", "1"}},
                }
            },
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .shader_info = {.source = daxa::ShaderFile{"compute.slang"}, .compile_options = {
                .entry_point = "entry_sphere_tracing",
            },},
#endif
            .push_constant_size = sizeof(ComputePush),
            .name = "sphere_tracing_compute_pipeline",
        }).value();
    }();
    // clang-format on

    // clang-format off
    std::shared_ptr<daxa::RayTracingPipeline> rt_pipeline = [this]() {
        update_virtual_shader();

        auto rt_gen_shader = daxa::ShaderCompileInfo{
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .source = daxa::ShaderFile{"raytracing.glsl"},
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .source = daxa::ShaderFile{"raytracing.slang"},
            .compile_options = {
                .entry_point = "rayGenShader",
            },
#endif
        };

        auto rt_miss_shader = daxa::ShaderCompileInfo{
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .source = daxa::ShaderFile{"raytracing.glsl"},
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .source = daxa::ShaderFile{"raytracing.slang"},
            .compile_options = {
                .entry_point = "missShader",
            },
#endif
        };

        auto rt_miss_shadow_shader = daxa::ShaderCompileInfo{
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .source = daxa::ShaderFile{"raytracing.glsl"},
            .compile_options = {
                .defines = std::vector{daxa::ShaderDefine{"DAXA_SHADOW_RAY_FLAG", "1"}},
            },
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .source = daxa::ShaderFile{"raytracing.slang"},
            .compile_options = {
                .entry_point = "missShadowShader",
            },
#endif
        };

        auto rt_closest_hit_shader = daxa::ShaderCompileInfo{
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .source = daxa::ShaderFile{"raytracing.glsl"},
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .source = daxa::ShaderFile{"raytracing.slang"},
            .compile_options = {
                .entry_point = "closestHitShader",
            },
#endif
        };

        auto rt_intersection_shader = daxa::ShaderCompileInfo{
#if DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL
            .source = daxa::ShaderFile{"raytracing.glsl"},  
#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG
            .source = daxa::ShaderFile{"raytracing.slang"},
            .compile_options = {
                .entry_point = "intersectionShader",
            },
#endif
        };

        return pipeline_manager.add_ray_tracing_pipeline({
        .ray_gen_infos = {
            rt_gen_shader
        },
        .intersection_infos = {
            rt_intersection_shader
        },
        .closest_hit_infos = {
            rt_closest_hit_shader
        },
        .miss_hit_infos = {
            rt_miss_shader,
            rt_miss_shadow_shader
        },
        // Groups are in order of their shader indices.
        // NOTE: The order of the groups is important! raygen, miss, hit, callable
        .shader_groups_infos = {
            daxa::RayTracingShaderGroupInfo{
                .type = daxa::ShaderGroup::GENERAL,
                .general_shader_index = 0,
            },
            daxa::RayTracingShaderGroupInfo{
                .type = daxa::ShaderGroup::GENERAL,
                .general_shader_index = 3,
            },
            daxa::RayTracingShaderGroupInfo{
                .type = daxa::ShaderGroup::GENERAL,
                .general_shader_index = 4,
            },
            daxa::RayTracingShaderGroupInfo{
                .type = daxa::ShaderGroup::PROCEDURAL_HIT_GROUP,
                .closest_hit_shader_index = 2,
                .intersection_shader_index = 1,
            },
        },
        .max_ray_recursion_depth = 2,
        .push_constant_size = sizeof(ComputePush),
        .name = "ray tracing pipeline",
    }).value();
    }();
    // clang-format on

    daxa::BufferId gpu_input_buffer = device.create_buffer(daxa::BufferInfo{
        .size = sizeof(GpuInput),
        .name = "gpu_input_buffer",
    });
    GpuInput gpu_input = { .p_count = NUM_PARTICLES, .grid_dim = {GRID_DIM, GRID_DIM, GRID_DIM}, .dt = 1e-4f, .dx = 1.0f / GRID_DIM, .inv_dx = GRID_DIM, .gravity = -9.8f};
    daxa::TaskBuffer task_gpu_input_buffer{{.initial_buffers = {.buffers = std::array{gpu_input_buffer}}, .name = "input_buffer"}};


    daxa::usize particles_size = NUM_PARTICLES * sizeof(Particle);
    daxa::BufferId particles_buffer = device.create_buffer(daxa::BufferInfo{
        .size = particles_size,
        .name = "particles_buffer",
    });
    daxa::TaskBuffer task_particles_buffer{{.initial_buffers = {.buffers = std::array{particles_buffer}}, .name = "particles_buffer_task"}};

    daxa::usize grid_size = GRID_SIZE * sizeof(Cell);
    daxa::BufferId grid_buffer = device.create_buffer(daxa::BufferInfo{
        .size = grid_size,
        .name = "grid_buffer",
    });
    daxa::TaskBuffer task_grid_buffer{{.initial_buffers = {.buffers = std::array{grid_buffer}}, .name = "grid_buffer_task"}};
    
    daxa::BufferClearInfo clear_info = {grid_buffer, 0, grid_size, 0};

    daxa::usize aabb_size = NUM_PARTICLES * sizeof(Aabb);
    daxa::BufferId aabb_buffer = device.create_buffer(daxa::BufferInfo{
        .size = aabb_size,
        .name = "aabb_buffer",
    });
    daxa::TaskBuffer task_aabb_buffer{{.initial_buffers = {.buffers = std::array{aabb_buffer}}, .name = "aabb_buffer_task"}};

    daxa::BufferId camera_buffer = device.create_buffer(daxa::BufferInfo{
        .size = sizeof(Camera),
        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .name = "camera_buffer",
    });
    daxa::TaskBuffer task_camera_buffer{{.initial_buffers = {.buffers = std::array{camera_buffer}}, .name = "camera_buffer_task"}};
    
    // daxa_u64 blas_scratch_buffer_size = 1024 * 1024 * 10;
    // daxa::TaskBuffer blas_scratch_buffer = daxa::TaskBuffer{device,{
    //                 .size = blas_scratch_buffer_size,
    //                 .name = ("blas_scratch_buffer_task"),
    //             }};
    // daxa_u64 blas_buffer_size = 1024 * 1024 * 10;
    // daxa::BufferId blas_buffer = device.create_buffer({
    //     .size = blas_buffer_size,
    //     .name = ("blas_buffer"),
    // });
    /// create blas instances for tlas:
    daxa::BufferId blas_instances_buffer = device.create_buffer({
        .size = sizeof(daxa_BlasInstanceData),
        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .name = "blas instances array buffer",
    });
    daxa::BlasBuildInfo blas_build_info = {};
    daxa::AccelerationStructureBuildSizesInfo tlas_build_sizes = {};
    daxa::AccelerationStructureBuildSizesInfo blas_build_sizes = {};
    daxa::TlasBuildInfo tlas_build_info = {};
    std::array<daxa::BlasAabbGeometryInfo, 1> geometry = {
        daxa::BlasAabbGeometryInfo{
            .data = device.get_device_address(aabb_buffer).value(),
            .stride = sizeof(daxa_f32mat3x2),
            .count = NUM_PARTICLES,
            .flags = daxa::GeometryFlagBits::OPAQUE, // Is also default
        }};
    // create blas instances info
    std::array<daxa::TlasInstanceInfo, 1> blas_instances = std::array{
            daxa::TlasInstanceInfo{
                .data = device.get_device_address(blas_instances_buffer).value(),
                .count = 1,
                .is_data_array_of_pointers = false,      // Buffer contains flat array of instances, not an array of pointers to instances.
                .flags = daxa::GeometryFlagBits::OPAQUE, // Is also default
            },
        };

    daxa::ImageId render_image = device.create_image(daxa::ImageInfo{
        .format = daxa::Format::R8G8B8A8_UNORM,
        .size = {size_x, size_y, 1},
        .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE | daxa::ImageUsageFlagBits::TRANSFER_SRC,
        .name = "render_image",
    });
    daxa::TaskImage task_render_image{{.initial_images = {.images = std::array{render_image}}, .name = "render_image"}};
    daxa::SamplerId sampler = device.create_sampler({.name = "sampler"});

    daxa::TaskGraph loop_task_graph = record_loop_task_graph();
    
    daxa::TaskGraph upload_task_graph = daxa::TaskGraph({
        .device = device,
        .use_split_barriers = false,
        .name = "upload_task_graph",
    });

    daxa::TaskGraph _input_task_graph = record_input_task_graph();

    daxa::TaskGraph _sim_task_graph = record_sim_task_graph();

    ~App()
    {
        device.wait_idle();
        device.collect_garbage();
        device.destroy_tlas(tlas);
        device.destroy_blas(blas);
        device.destroy_sampler(sampler);
        device.destroy_buffer(gpu_input_buffer);
        device.destroy_buffer(particles_buffer);
        device.destroy_buffer(grid_buffer);
        device.destroy_buffer(aabb_buffer);
        device.destroy_buffer(camera_buffer);
        // device.destroy_buffer(blas_buffer);
        device.destroy_buffer(blas_instances_buffer);
        device.destroy_image(render_image);
    }

    void ui_update()
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Begin("Settings");

        ImGui::Image(
            imgui_renderer.create_texture_id({
                .image_view_id = render_image.default_view(),
                .sampler_id = sampler,
            }),
            ImVec2(200, 200));

        if (ImGui::Checkbox("MY_TOGGLE", &my_toggle))
        {
            update_virtual_shader();
        }
        ImGui::End();
        ImGui::Render();
    }
    void on_update()
    {
        // gpu_input.time = time;
        // gpu_input.delta_time = delta_time;

        auto reloaded_result = pipeline_manager.reload_all();
        if (auto reload_err = daxa::get_if<daxa::PipelineReloadError>(&reloaded_result))
            std::cout << "Failed to reload " << reload_err->message << '\n';
        if (daxa::get_if<daxa::PipelineReloadSuccess>(&reloaded_result))
            std::cout << "Successfully reloaded!\n";

        ui_update();

        auto swapchain_image = swapchain.acquire_next_image();
        task_swapchain_image.set_images({.images = std::array{swapchain_image}});
        if (swapchain_image.is_empty())
        {
            return;
        }
    
        Camera camera = {
            .inv_view = glm_mat4_to_daxa_f32mat4x4(get_inverse_view_matrix(cam)),
            .inv_proj = glm_mat4_to_daxa_f32mat4x4(get_inverse_projection_matrix(cam)),
            .frame_dim = {size_x, size_y},
        };

        // NOTE: Vulkan has inverted y axis in NDC
        camera.inv_proj.y.y *= -1;
        
        device.get_host_address_as<Camera>(camera_buffer).value()[0] = camera;

        build_accel_structs();
        // loop_task_graph = record_loop_task_graph();
        loop_task_graph.execute({});
        device.collect_garbage();
    }

    void on_mouse_move(f32 /*unused*/, f32 /*unused*/) {}
    void on_mouse_button(i32 /*unused*/, i32 /*unused*/) {}
    void on_key(i32 /*unused*/, i32 /*unused*/) {}
    void on_resize(u32 sx, u32 sy) 
    {
        minimized = (sx == 0 || sy == 0);
        if (!minimized)
        {
            swapchain.resize();
            size_x = swapchain.get_surface_extent().x;
            size_y = swapchain.get_surface_extent().y;
            device.destroy_image(render_image);
            render_image = device.create_image({
                .format = daxa::Format::R8G8B8A8_UNORM,
                .size = {size_x, size_y, 1},
                .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE | daxa::ImageUsageFlagBits::TRANSFER_SRC,
            });
            task_render_image.set_images({.images = std::array{render_image}});
            
            camera_set_aspect(cam, size_x, size_y);
            base_on_update();
        }
    }

    void particle_set_position() {
        upload_task_graph.use_persistent_buffer(task_particles_buffer);
        upload_task_graph.use_persistent_buffer(task_aabb_buffer);

        upload_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::HOST_TRANSFER_WRITE, task_particles_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_aabb_buffer),
            },
            .task = [this](daxa::TaskInterface ti)
            {
                auto staging_particles_buffer = device.create_buffer({
                    .size = particles_size,
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = ("staging_particles_buffer"),
                });
                ti.recorder.destroy_buffer_deferred(staging_particles_buffer);
                auto * particles_ptr = device.get_host_address_as<Particle>(staging_particles_buffer).value();

                auto staging_aabb_buffer = device.create_buffer({
                    .size = aabb_size,
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = ("staging_particles_buffer"),
                });
                ti.recorder.destroy_buffer_deferred(staging_aabb_buffer);
                auto * aabb_ptr = device.get_host_address_as<Aabb>(staging_aabb_buffer).value();

                srand(static_cast<unsigned int>(std::time(NULL)));
                float min_bound = 0.1f;
                float max_bound = 0.9f;

                for (u32 i = 0; i < NUM_PARTICLES; i++)
                {
                    particles_ptr[i] = {
                        .type = MAT_WATER,
                        .v = {0.0f, 0.0f, 0.0f},
                        .F = make_identity(),
                        .C = make_zero(),
                        .J = 1.0f,
                    };
                    
                    
                    daxa_f32vec3 x = {
                        random_in_range(min_bound + PARTICLE_RADIUS, max_bound - PARTICLE_RADIUS), 
                        random_in_range(min_bound + PARTICLE_RADIUS, max_bound - PARTICLE_RADIUS), 
                        random_in_range(min_bound + PARTICLE_RADIUS, max_bound - PARTICLE_RADIUS)
                    };

                    aabb_ptr[i] = {
                        .min = x - daxa_f32vec3{PARTICLE_RADIUS, PARTICLE_RADIUS, PARTICLE_RADIUS},
                        .max = x + daxa_f32vec3{PARTICLE_RADIUS, PARTICLE_RADIUS, PARTICLE_RADIUS},
                    };

                }
                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_particles_buffer,
                    .dst_buffer = particles_buffer,
                    .size = particles_size,
                });

                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_aabb_buffer,
                    .dst_buffer = aabb_buffer,
                    .size = aabb_size,
                });
            },
            .name = ("Upload particles"),
        });

        upload_task_graph.submit({});
        upload_task_graph.complete({});
        upload_task_graph.execute({});
    }


    daxa::TaskGraph record_input_task_graph()
    {
        daxa::TaskGraph input_task_graph = daxa::TaskGraph({
            .device = device,
            .use_split_barriers = false,
            .name = "input_task_graph",
        });

        input_task_graph.use_persistent_image(task_render_image);
        input_task_graph.use_persistent_buffer(task_gpu_input_buffer);
        input_task_graph.use_persistent_buffer(task_particles_buffer);
        input_task_graph.use_persistent_buffer(task_grid_buffer);
        input_task_graph.use_persistent_buffer(task_camera_buffer);
        
        reset_camera(cam);

        input_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::HOST_TRANSFER_WRITE, task_gpu_input_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::HOST_TRANSFER_WRITE, task_particles_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_grid_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::HOST_TRANSFER_WRITE, task_camera_buffer),
            },
            .task = [this](daxa::TaskInterface ti)
            {
                auto staging_gpu_input_buffer = device.create_buffer({
                    .size = sizeof(GpuInput),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = ("staging_gpu_input_buffer"),
                });
                ti.recorder.destroy_buffer_deferred(staging_gpu_input_buffer);
                auto * buffer_ptr = device.get_host_address_as<GpuInput>(staging_gpu_input_buffer).value();
                *buffer_ptr = gpu_input;
                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_gpu_input_buffer,
                    .dst_buffer = gpu_input_buffer,
                    .size = sizeof(GpuInput),
                });
            },
            .name = ("Upload Input"),
        });

        input_task_graph.submit({});
        input_task_graph.complete({});
        
        return input_task_graph;
    }


    daxa::TaskGraph record_sim_task_graph() {
        daxa::TaskGraph sim_task_graph = daxa::TaskGraph({
            .device = device,
            .use_split_barriers = false,
            .name = "sim_task_graph",
        });

        sim_task_graph.use_persistent_image(task_render_image);
        sim_task_graph.use_persistent_buffer(task_gpu_input_buffer);
        sim_task_graph.use_persistent_buffer(task_particles_buffer);
        sim_task_graph.use_persistent_buffer(task_grid_buffer);
        sim_task_graph.use_persistent_buffer(task_aabb_buffer);
        sim_task_graph.use_persistent_buffer(task_camera_buffer);

        sim_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_gpu_input_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_grid_buffer),
            },
            .task = [this](daxa::TaskInterface ti)
            {
                ti.recorder.clear_buffer(clear_info);
            },
            .name = ("Reset Grid (Compute)"),
        });
        sim_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_gpu_input_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ_WRITE, task_particles_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ_WRITE, task_grid_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_camera_buffer),
                daxa::inl_attachment(daxa::TaskImageAccess::COMPUTE_SHADER_STORAGE_WRITE_ONLY, task_render_image),
            },
            .task = [this](daxa::TaskInterface ti)
            {
                ti.recorder.set_pipeline(*p2g_compute_pipeline);
                ti.recorder.push_constant(ComputePush{
                    .image_id = render_image.default_view(),
                    .input_buffer_id = gpu_input_buffer,
                    .input_ptr = device.get_device_address(gpu_input_buffer).value(),
                    .particles = device.get_device_address(particles_buffer).value(),
                    .cells = device.get_device_address(grid_buffer).value(),
                    .aabbs = device.get_device_address(aabb_buffer).value(),
                    .camera = device.get_device_address(camera_buffer).value(),
                    .tlas = tlas,
                });
                ti.recorder.dispatch({(gpu_input.p_count + MPM_P2G_COMPUTE_X - 1) / MPM_P2G_COMPUTE_X});
            },
            .name = ("P2G (Compute)"),
        });
        sim_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_gpu_input_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ_WRITE, task_particles_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ_WRITE, task_grid_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_camera_buffer),
                daxa::inl_attachment(daxa::TaskImageAccess::COMPUTE_SHADER_STORAGE_WRITE_ONLY, task_render_image),
            },
            .task = [this](daxa::TaskInterface ti)
            {
                ti.recorder.set_pipeline(*grid_compute_pipeline);

                ti.recorder.push_constant(ComputePush{
                    .image_id = render_image.default_view(),
                    .input_buffer_id = gpu_input_buffer,
                    .input_ptr = device.get_device_address(gpu_input_buffer).value(),
                    .particles = device.get_device_address(particles_buffer).value(),
                    .cells = device.get_device_address(grid_buffer).value(),
                    .aabbs = device.get_device_address(aabb_buffer).value(),
                    .camera = device.get_device_address(camera_buffer).value(),
                    .tlas = tlas,
                });
                ti.recorder.dispatch({(gpu_input.grid_dim.x + MPM_GRID_COMPUTE_X - 1) / MPM_GRID_COMPUTE_X, (gpu_input.grid_dim.y + MPM_GRID_COMPUTE_X - 1) / MPM_GRID_COMPUTE_Y, (gpu_input.grid_dim.z + MPM_GRID_COMPUTE_Z - 1) / MPM_GRID_COMPUTE_Z});
            },
            .name = ("Grid (Compute)"),
        });
        sim_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_gpu_input_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ_WRITE, task_particles_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ_WRITE, task_grid_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_camera_buffer),
                daxa::inl_attachment(daxa::TaskImageAccess::COMPUTE_SHADER_STORAGE_WRITE_ONLY, task_render_image),
            },
            .task = [this](daxa::TaskInterface ti)
            {
                ti.recorder.set_pipeline(*g2p_compute_pipeline);

                ti.recorder.push_constant(ComputePush{
                    .image_id = render_image.default_view(),
                    .input_buffer_id = gpu_input_buffer,
                    .input_ptr = device.get_device_address(gpu_input_buffer).value(),
                    .particles = device.get_device_address(particles_buffer).value(),
                    .cells = device.get_device_address(grid_buffer).value(),
                    .aabbs = device.get_device_address(aabb_buffer).value(),
                    .camera = device.get_device_address(camera_buffer).value(),
                    .tlas = tlas,
                });
                ti.recorder.dispatch({(gpu_input.p_count + MPM_P2G_COMPUTE_X - 1) / MPM_P2G_COMPUTE_X});
            },
            .name = ("G2P (Compute)"),
        });
        sim_task_graph.submit({});
        sim_task_graph.complete({});

        return sim_task_graph;
    }

    void update_input_task()
    {
        _input_task_graph.execute({});
    }

    void update_sim() {
        for(int i = 0; i < SIM_LOOP_COUNT; i++) {
            _sim_task_graph.execute({});
            device.wait_idle();
        }
    }

    void build_accel_structs() {  

        // BUILDING BLAS

        if(!blas.is_empty()) {
            device.destroy_blas(blas);
            // ti.device.destroy_buffer(blas_buffer);
        }
        blas_build_info = daxa::BlasBuildInfo{
            .flags = daxa::AccelerationStructureBuildFlagBits::PREFER_FAST_BUILD, // Is also default
            .dst_blas = {},                                                       // Ignored in get_acceleration_structure_build_sizes.       // Is also default
            .geometries = geometry,
            .scratch_data = {}, // Ignored in get_acceleration_structure_build_sizes.   // Is also default
        };
        blas_build_sizes = device.get_blas_build_sizes(blas_build_info);
        // blas_build_info.scratch_data = device.get_device_address(blas_scratch_buffer.get_state().buffers[0]).value();
        
        // blas = device.create_blas_from_buffer({
        //     .blas_info = {   .size = blas_build_sizes.acceleration_structure_size,
        //         .name = "blas",
        //     },
        //     .buffer_id = blas_buffer,
        //     .offset = 0, // TODO: for more than one BLAS
        // });

        blas = device.create_blas({
            .size = blas_build_sizes.acceleration_structure_size,
            .name = "blas",
        });

        blas_build_info.dst_blas = blas;

        task_blas.set_blas({.blas = std::array{blas}});


        // BUILDIING TLAS

        if (!tlas.is_empty())
        {
            device.destroy_tlas(tlas);
        }

        // define blas instances
        auto blas_instance_array = std::array{
            daxa_BlasInstanceData{
                .transform = {
                    {1, 0, 0, 0},
                    {0, 1, 0, 0},
                    {0, 0, 1, 0},
                },
                .instance_custom_index = 0, // Is also default
                .mask = 0xFF,
                .instance_shader_binding_table_record_offset = 0,
                .flags = {}, // Is also default
                .blas_device_address = device.get_device_address(blas).value(),
            }};

        // copy blas instances to buffer
        std::memcpy(device.get_host_address_as<daxa_BlasInstanceData>(blas_instances_buffer).value(),
                    blas_instance_array.data(),
                    blas_instance_array.size() * sizeof(daxa_BlasInstanceData));

        // build tlas info
        tlas_build_info = daxa::TlasBuildInfo{
            .flags = daxa::AccelerationStructureBuildFlagBits::PREFER_FAST_BUILD,
            .dst_tlas = {}, // Ignored in get_acceleration_structure_build_sizes.
            .instances = blas_instances,
            .scratch_data = {}, // Ignored in get_acceleration_structure_build_sizes.
        };

        // tlas build info
        tlas_build_sizes = device.get_tlas_build_sizes(tlas_build_info);

        // tlas struct
        tlas = device.create_tlas({
            .size = tlas_build_sizes.acceleration_structure_size,
            .name = "tlas",
        });
        tlas_build_info.dst_tlas = tlas;

        task_tlas.set_tlas({.tlas = std::array{tlas}});
    }

    void record_accel_struct_tasks(daxa::TaskGraph & new_task_graph) {
        new_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, task_aabb_buffer),
                daxa::inl_attachment(daxa::TaskBlasAccess::BUILD_WRITE, task_blas),
            },
            .task = [this](daxa::TaskInterface const &ti) {
                // create scratch buffer
                auto blas_scratch_buffer = device.create_buffer({
                    .size = blas_build_sizes.build_scratch_size,
                    .name = "blas build scratch buffer",
                });
                // add to deferred destruction
                ti.recorder.destroy_buffer_deferred(blas_scratch_buffer);
                // attach scratch buffer to task
                blas_build_info.scratch_data = device.get_device_address(blas_scratch_buffer).value();
                // build blas
                ti.recorder.build_acceleration_structures({
                    .blas_build_infos = std::array{blas_build_info},
                });
            },
            .name = "blas build",
        });
        new_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBlasAccess::BUILD_READ, task_blas),
                daxa::inl_attachment(daxa::TaskTlasAccess::BUILD_WRITE, task_tlas),
            },
            .task = [this](daxa::TaskInterface const &ti) {
                // create scratch buffer
                auto tlas_scratch_buffer = device.create_buffer({
                    .size = tlas_build_sizes.build_scratch_size,
                    .name = "tlas build scratch buffer",
                });
                // add to deferred destruction
                ti.recorder.destroy_buffer_deferred(tlas_scratch_buffer);
                // attach scratch buffer to task
                tlas_build_info.scratch_data = device.get_device_address(tlas_scratch_buffer).value();
                // build tlas
                ti.recorder.build_acceleration_structures({
                    .tlas_build_infos = std::array{tlas_build_info},
                });
            },
            .name = "tlas build",
        });
    }

    void record_tasks(daxa::TaskGraph & new_task_graph)
    {
        new_task_graph.use_persistent_image(task_render_image);
        new_task_graph.use_persistent_buffer(task_gpu_input_buffer);
        new_task_graph.use_persistent_buffer(task_particles_buffer);
        new_task_graph.use_persistent_buffer(task_grid_buffer);
        new_task_graph.use_persistent_buffer(task_aabb_buffer);
        new_task_graph.use_persistent_buffer(task_camera_buffer);
        new_task_graph.use_persistent_blas(task_blas);
        new_task_graph.use_persistent_tlas(task_tlas);

        imgui_task_attachments.push_back(daxa::inl_attachment(daxa::TaskImageAccess::FRAGMENT_SHADER_SAMPLED, task_render_image));
        record_accel_struct_tasks(new_task_graph);
        // new_task_graph.add_task({
        //     .attachments = {
        //         daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_gpu_input_buffer),
        //         daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ_WRITE, task_particles_buffer),
        //         daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ_WRITE, task_grid_buffer),
        //         daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_camera_buffer),
        //         daxa::inl_attachment(daxa::TaskImageAccess::COMPUTE_SHADER_STORAGE_WRITE_ONLY, task_render_image),
        //     },
        //     .task = [this](daxa::TaskInterface ti)
        //     {
        //         ti.recorder.set_pipeline(*sphere_tracing_compute_pipeline);

        //         ti.recorder.push_constant(ComputePush{
        //             .image_id = render_image.default_view(),
        //             .input_buffer_id = gpu_input_buffer,
        //             .input_ptr = device.get_device_address(gpu_input_buffer).value(),
        //             .particles = device.get_device_address(particles_buffer).value(),
        //             .cells = device.get_device_address(grid_buffer).value(),
        //             .aabbs = device.get_device_address(aabb_buffer).value(),
        //             .camera = device.get_device_address(camera_buffer).value(),
        //             .tlas = tlas,
        //         });
        //         ti.recorder.dispatch({(size_x + MPM_SHADING_COMPUTE_X - 1) / MPM_SHADING_COMPUTE_X, (size_y + MPM_SHADING_COMPUTE_Y - 1) / MPM_SHADING_COMPUTE_Y});
        //     },
        //     .name = ("Draw (Compute)"),
        // });
        new_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::RAY_TRACING_SHADER_READ, task_gpu_input_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::RAY_TRACING_SHADER_READ_WRITE, task_particles_buffer),
                daxa::inl_attachment(daxa::TaskBufferAccess::RAY_TRACING_SHADER_READ, task_camera_buffer),
                daxa::inl_attachment(daxa::TaskImageAccess::RAY_TRACING_SHADER_STORAGE_WRITE_ONLY, task_render_image),
                daxa::inl_attachment(daxa::TaskTlasAccess::BUILD_READ, task_tlas),
            },
            .task = [this](daxa::TaskInterface ti)
            {
                ti.recorder.set_pipeline(*rt_pipeline);

                ti.recorder.push_constant(ComputePush{
                    .image_id = render_image.default_view(),
                    .input_buffer_id = gpu_input_buffer,
                    .input_ptr = device.get_device_address(gpu_input_buffer).value(),
                    .particles = device.get_device_address(particles_buffer).value(),
                    .cells = device.get_device_address(grid_buffer).value(),
                    .aabbs = device.get_device_address(aabb_buffer).value(),
                    .camera = device.get_device_address(camera_buffer).value(),
                    .tlas = tlas,
                });
                ti.recorder.trace_rays({
                    .width = size_x,
                    .height = size_y,
                    .depth = 1,
                });
            },
            .name = ("Draw (Ray Tracing)"),
        });
        new_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_READ, task_render_image),
                daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, task_swapchain_image),
            },
            .task = [this](daxa::TaskInterface ti)
            {
                ti.recorder.blit_image_to_image({
                    .src_image = ti.get(task_render_image).ids[0],
                    .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                    .dst_image = ti.get(task_swapchain_image).ids[0],
                    .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                    .src_offsets = {{{0, 0, 0}, {static_cast<i32>(size_x), static_cast<i32>(size_y), 1}}},
                    .dst_offsets = {{{0, 0, 0}, {static_cast<i32>(size_x), static_cast<i32>(size_y), 1}}},
                });
            },
            .name = "Blit (render to swapchain)",
        });
    }
};

auto main() -> int
{
    App app = {};
    app.particle_set_position();
    while (true)
    {
        app.update_input_task();
        app.update_sim();
        // app.update_accel_struct_task();
        if (app.update())
        {
            break;
        }
    }
}
