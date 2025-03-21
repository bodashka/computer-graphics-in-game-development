#include "raytracer_renderer.h"

#include "renderer/raytracer/raytracer.h"
#include "utils/resource_utils.h"

#include <chrono>
#include <iostream>
#include <random>

void cg::renderer::ray_tracing_renderer::init()
{

	std::cout << "Initializing ray tracing" << std::endl;
	
	// TODO Lab: 2.01 Add `render_target`, `camera`, and `raytracer` in `ray_tracing_renderer` class
	model = std::make_shared<cg::world::model>();
	model->load_obj(settings->model_path);

	camera = std::make_shared<cg::world::camera>();
	camera->set_height(static_cast<float>(settings->height));
	camera->set_width(static_cast<float>(settings->width));
	camera->set_position(float3{
			settings->camera_position[0],
			settings->camera_position[1],
			settings->camera_position[2],
	});
	camera->set_phi(settings->camera_phi);
	camera->set_theta(settings->camera_theta);
	camera->set_angle_of_view(settings->camera_angle_of_view);
	camera->set_z_far(settings->camera_z_far);
	camera->set_z_near(settings->camera_z_near);

	render_target = std::make_shared<cg::resource<cg::unsigned_color>>(settings->width, settings->height);

	raytracer = std::make_shared<cg::renderer::raytracer<cg::vertex, cg::unsigned_color>>();
	raytracer->set_render_target(render_target);
	raytracer->set_viewport(settings->width, settings->height);
	raytracer->set_index_buffers(model->get_index_buffers());
	raytracer->set_vertex_buffers(model->get_vertex_buffers());

	// Lab: 2.03 Add light information to `lights` array of `ray_tracing_renderer`

	lights.push_back({
			float3{0.f, 1.58f, -0.03f},
			float3{0.78f, 0.78f, 0.78f},
	});

	// Lab: 2.04 Initialize `shadow_raytracer` in `ray_tracing_renderer`
	shadow_raytracer = std::make_shared<cg::renderer::raytracer<cg::vertex, cg::unsigned_color>>(); 
	shadow_raytracer->set_index_buffers(model->get_index_buffers());
	shadow_raytracer->set_vertex_buffers(model->get_vertex_buffers());

	std::cout << "Init raytracer end" << std::endl;
}

void cg::renderer::ray_tracing_renderer::destroy() {}

void cg::renderer::ray_tracing_renderer::update() {}

void cg::renderer::ray_tracing_renderer::render()
{
	std::cout << "Rendering..." << std::endl;
	// Lab: 2.04 Define `any_hit_shader` and `miss_shader` for `shadow_raytracer`
	shadow_raytracer->miss_shader = [](const ray& ray) {
		payload payload{};
		payload.t = -1.f;
		return payload;
	};
	shadow_raytracer->any_hit_shader = [](const ray& ray, payload& payload,
										  const triangle<cg::vertex>& triangle) {
		return payload;
	};
	shadow_raytracer->build_acceleration_structure();

	// Lab: 2.01 Implement `miss_shader`, image clearing, calling `ray_generation`, and saving in `ray_tracing_renderer` class
	raytracer->clear_render_target({0, 0, 0});
	raytracer->miss_shader = [](const ray& ray) {
		payload payload{};
		//payload.color = {0.f, 0.f, (ray.direction.y + 1.f) * 0.5f};
		payload.color = {0.f, 0.f, 0.f};
		return payload;
	};
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis(-1.f, 1.f);

	// Lab: 2.02 Add `closest_hit_shader` to `raytracer` class to return diffuse color
	raytracer->closest_hit_shader = [&](const ray& ray, payload& payload,
										const triangle<cg::vertex>& triangle, size_t depth) {

		// Lab: 2.03 Adjust `closest_hit_shader` of `raytracer` to implement Lambertian shading model

		float3 position = ray.position + ray.direction * payload.t;
		float3 normal = normalize(
				payload.bary.x * triangle.na +
				payload.bary.y * triangle.nb +
				payload.bary.z * triangle.nc);
		float3 result_color = triangle.emissive;

		/*for (auto light: lights){
			cg::renderer::ray to_light(position, light.position - position);
			// -- Lab 04 segment
			auto shadow = shadow_raytracer->trace_ray(to_light, 1, length(light.position - position));
			if (shadow.t < 0.f)
				result_color += triangle.diffuse * light.color * std::max(dot(normal, to_light.direction), 0.f);
		}*/

		float3 random_direction{dis(gen), dis(gen), dis(gen)};
		if (dot(normal, random_direction) < 0.f)
			random_direction = -random_direction;

		for (auto& light: lights) {
			cg::renderer::ray to_next_object(position, random_direction);
			auto next_payload = raytracer->trace_ray(to_next_object, depth);
			result_color += triangle.diffuse * next_payload.color.to_float3() * std::max(dot(normal, to_next_object.direction), 0.f);
		}

		//payload.color = cg::color::from_float3(triangle.diffuse);
		payload.color = cg::color::from_float3(result_color);
		return payload;
	};
	raytracer->build_acceleration_structure();

	auto start = std::chrono::system_clock::now();

	raytracer->ray_generation(
			camera->get_position(), camera->get_direction(),
			camera->get_right(), camera->get_up(),
			settings->raytracing_depth,
			settings->accumulation_num);

	auto stop = std::chrono::system_clock::now();
	std::chrono::duration<float, std::milli> raytracing_duration = stop - start;
	std::cout << "Raytracing took " << raytracing_duration.count() << " ms\n";

	cg::utils::save_resource(*render_target, settings->result_path);
	// TODO Lab: 2.04 Adjust `closest_hit_shader` of `raytracer` to cast shadows rays and to ignore occluded lights
	// TODO Lab: 2.05 Adjust `ray_tracing_renderer` class to build the acceleration structure
	// TODO Lab: 2.06 (Bonus) Adjust `closest_hit_shader` for Monte-Carlo light tracing
}