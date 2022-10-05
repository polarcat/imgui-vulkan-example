/* Copyright (C) 2022 Aliaksei Katovich. All rights reserved.
 *
 * This source code is licensed under the BSD Zero Clause License found in
 * the 0BSD file in the root directory of this source tree.
 */

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "utils.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#ifndef APP_NAME
#define APP_NAME "main"
#endif

struct context {
	GLFWwindow *win;
	ImGui_ImplVulkanH_Window gui_win;
	VkInstance vk_instance = VK_NULL_HANDLE;
	VkPhysicalDevice vk_gpu = VK_NULL_HANDLE;
	VkDevice vk_dev = VK_NULL_HANDLE;
	VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
	VkPipelineCache vk_pipeline_cache = VK_NULL_HANDLE;
	VkQueue vk_queue = VK_NULL_HANDLE;
	uint32_t vk_queue_family = (uint32_t) -1;
	VkAllocationCallbacks *vk_alloctor = NULL;
	int image_count = 2;
	bool rebuild_swapchain = false;
	uint8_t font_size = 24;
	VkDebugReportCallbackEXT vk_debug = VK_NULL_HANDLE;
	int w = 640;
	int h = 480;
	bool show_demo_window = true;
};

static struct context ctx_;

static VkResult create_instance(struct context *ctx)
{
	VkResult err;
	uint32_t cnt = 0;
	const char **ext = glfwGetRequiredInstanceExtensions(&cnt);

	VkInstanceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.enabledExtensionCount = cnt;
	info.ppEnabledExtensionNames = ext;

	vk_call(err, vkCreateInstance(&info, ctx->vk_alloctor,
	 &ctx->vk_instance));
	return err;
}

static VkResult select_gpu(struct context *ctx)
{
	VkResult err;
        uint32_t cnt;
        VkPhysicalDevice *gpus;
        uint32_t gpu;

        vk_call(err, vkEnumeratePhysicalDevices(ctx->vk_instance, &cnt,
	 NULL));
	if (cnt == 0)
		return VK_NOT_READY;

	size_t bytes = sizeof(VkPhysicalDevice) * cnt;

        if (!(gpus = (VkPhysicalDevice *) malloc(bytes))) {
		ee("failed to allocate %zu bytes\n", bytes);
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

        vk_call(err, vkEnumeratePhysicalDevices(ctx->vk_instance, &cnt,
	 gpus));
	if (err != VK_SUCCESS)
		goto out;

	/* find discrete GPU; use first if not found */
	gpu = 0;
        for (uint32_t i = 0; i < cnt; ++i) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(gpus[i], &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                gpu = i;
                break;
            }
        }

out:
        ctx->vk_gpu = gpus[gpu];
        free(gpus);
	return err;
}

static VkResult select_queue_family(struct context *ctx)
{
	VkResult err;
        uint32_t cnt;
        VkQueueFamilyProperties *queues;

        vkGetPhysicalDeviceQueueFamilyProperties(ctx->vk_gpu, &cnt, NULL);
	if (cnt == 0)
		return VK_ERROR_INITIALIZATION_FAILED;

	size_t bytes = sizeof(VkQueueFamilyProperties) * cnt;

        if (!(queues = (VkQueueFamilyProperties *) malloc(bytes))) {
		ee("failed to allocate %zu bytes\n", bytes);
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

        vkGetPhysicalDeviceQueueFamilyProperties(ctx->vk_gpu, &cnt, queues);

        for (uint32_t i = 0; i < cnt; ++i) {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                ctx->vk_queue_family = i;
                break;
            }
	}

out:
        free(queues);

	if (ctx->vk_queue_family != (uint32_t) -1) {
		err = VK_SUCCESS;
	} else {
		ee("failed to select vulkan queue family\n");
		err = VK_ERROR_INITIALIZATION_FAILED;
	}

	return err;
}

static VkResult create_device(struct context *ctx)
{
	VkResult err;
        const char *ext[] = { "VK_KHR_swapchain" };
        int ext_cnt = ARRAY_SIZE(ext);
        const float queue_priority[] = { 1. };

        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = ctx->vk_queue_family;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;

        VkDeviceCreateInfo dev_info = {};
        dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dev_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        dev_info.pQueueCreateInfos = queue_info;
        dev_info.enabledExtensionCount = ext_cnt;
        dev_info.ppEnabledExtensionNames = ext;

        vk_call(err, vkCreateDevice(ctx->vk_gpu, &dev_info,
	 ctx->vk_alloctor, &ctx->vk_dev));
	if (err != VK_SUCCESS)
		return err;

        vkGetDeviceQueue(ctx->vk_dev, ctx->vk_queue_family, 0,
	 &ctx->vk_queue);
	return err;
}

static VkResult create_descriptor_pool(struct context *ctx)
{
	VkResult err;
        VkDescriptorPoolSize sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets = 1000 * ARRAY_SIZE(sizes);
        info.poolSizeCount = (uint32_t) ARRAY_SIZE(sizes);
        info.pPoolSizes = sizes;

        vk_call(err, vkCreateDescriptorPool(ctx->vk_dev, &info,
	 ctx->vk_alloctor, &ctx->vk_descriptor_pool));
	return err;
}

static bool init_vulkan(struct context *ctx)
{
	if (create_instance(ctx) != VK_SUCCESS)
		return false;
	else if (select_gpu(ctx) != VK_SUCCESS)
		return false;
	else if (select_queue_family(ctx) != VK_SUCCESS)
		return false;
	else if (create_device(ctx) != VK_SUCCESS)
		return false;
	else if (create_descriptor_pool(ctx) != VK_SUCCESS)
		return false;
	else
		return true;
}

static bool init_surface(struct context *ctx)
{
	VkResult err;

	vk_call(err, glfwCreateWindowSurface(ctx->vk_instance,
	 ctx->win, ctx->vk_alloctor, &ctx->gui_win.Surface));
	if (err != VK_SUCCESS) {
		ee("failed to create window surface\n");
		return false;
	}

	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(ctx->vk_gpu,
	 ctx->vk_queue_family, ctx->gui_win.Surface, &res);
	if (res != VK_TRUE) {
		ee("WSI is not supported\n");
		return false;
	}

	glfwGetFramebufferSize(ctx->win, &ctx->w, &ctx->h);
	ii("framebuffer size (%d %d)\n", ctx->w, ctx->h);
	return true;
}

static bool init_window(struct context *ctx)
{
	const char *geom = getenv("WIN_SIZE");

	if (geom) {
		const char *h_str;
		ctx->w = atoi(geom);
		if (!(h_str = strchr(geom, 'x'))) {
			ww("malformed WIN_SIZE, use default height\n");
		} else {
			ctx->h = atoi(h_str + 1);
		}
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	ctx->win = glfwCreateWindow(ctx->w, ctx->h, APP_NAME, NULL, NULL);
	if (!ctx->win) {
		ee("failed to create window\n");
		return false;
	}

	if (!glfwVulkanSupported()) {
		ee("GLFW does not support vulkan\n");
		return false;
	}

	return true;
}

static void cleanup_vulkan(struct context *ctx)
{
	vkDestroyDescriptorPool(ctx->vk_dev, ctx->vk_descriptor_pool,
	 ctx->vk_alloctor);
	vkDestroyDevice(ctx->vk_dev, ctx->vk_alloctor);
	vkDestroyInstance(ctx->vk_instance, ctx->vk_alloctor);
}

static void render_frame(struct context *ctx, ImDrawData* draw_data)
{
	VkResult err;
	ImGui_ImplVulkanH_Window *win = &ctx->gui_win;
	uint8_t sem_idx = uint8_t(win->SemaphoreIndex);

	VkSemaphore img_sem;
	img_sem = win->FrameSemaphores[sem_idx].ImageAcquiredSemaphore;
	VkSemaphore rend_sem;
	rend_sem = win->FrameSemaphores[sem_idx].RenderCompleteSemaphore;

	vk_call(err, vkAcquireNextImageKHR(ctx->vk_dev, win->Swapchain,
	 UINT64_MAX, img_sem, VK_NULL_HANDLE, &win->FrameIndex));
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
		ctx->rebuild_swapchain = true;
		return;
	} else if (err != VK_SUCCESS) {
		return;
	}

	ImGui_ImplVulkanH_Frame *fd = &win->Frames[win->FrameIndex];

	/* blocking call */
	vk_call(err, vkWaitForFences(ctx->vk_dev, 1, &fd->Fence, VK_TRUE,
	 UINT64_MAX));
	if (err != VK_SUCCESS)
		return;

	vk_call(err, vkResetFences(ctx->vk_dev, 1, &fd->Fence));
	if (err != VK_SUCCESS)
		return;

	vk_call(err, vkResetCommandPool(ctx->vk_dev, fd->CommandPool, 0));
	if (err != VK_SUCCESS)
		return;

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vk_call(err, vkBeginCommandBuffer(fd->CommandBuffer, &begin_info));
	if (err != VK_SUCCESS)
		return;

	VkRenderPassBeginInfo pass_info = {};
	pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	pass_info.renderPass = win->RenderPass;
	pass_info.framebuffer = fd->Framebuffer;
	pass_info.renderArea.extent.width = win->Width;
	pass_info.renderArea.extent.height = win->Height;
	pass_info.clearValueCount = 1;
	pass_info.pClearValues = &win->ClearValue;

	vkCmdBeginRenderPass(fd->CommandBuffer, &pass_info,
	 VK_SUBPASS_CONTENTS_INLINE);

	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);
	vkCmdEndRenderPass(fd->CommandBuffer);
	vk_call(err, vkEndCommandBuffer(fd->CommandBuffer));
	if (err != VK_SUCCESS)
		return;

	VkPipelineStageFlags wait_stage;
	wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo queue_info = {};
	queue_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queue_info.waitSemaphoreCount = 1;
	queue_info.pWaitSemaphores = &img_sem;
	queue_info.pWaitDstStageMask = &wait_stage;
	queue_info.commandBufferCount = 1;
	queue_info.pCommandBuffers = &fd->CommandBuffer;
	queue_info.signalSemaphoreCount = 1;
	queue_info.pSignalSemaphores = &rend_sem;

	vk_call(err, vkQueueSubmit(ctx->vk_queue, 1, &queue_info, fd->Fence));
	if (err != VK_SUCCESS)
		return;
}

static void present_gui(struct context *ctx)
{
	VkResult err;

	if (ctx->rebuild_swapchain)
		return;

	ImGui_ImplVulkanH_Window *win = &ctx->gui_win;
	uint8_t i = uint8_t(win->SemaphoreIndex);
	VkSemaphore sem = win->FrameSemaphores[i].RenderCompleteSemaphore;

	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &sem;
	info.swapchainCount = 1;
	info.pSwapchains = &win->Swapchain;
	info.pImageIndices = &win->FrameIndex;

	err = vkQueuePresentKHR(ctx->vk_queue, &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
		ctx->rebuild_swapchain = true;
		return;
	} else if (err != VK_SUCCESS) {
		ee("queue present failed, %s\n", vk_strerror(err));
		return;
	}

	/* use the next set of semaphores */
	win->SemaphoreIndex += 1;
	win->SemaphoreIndex %= win->ImageCount;
}

static void vk_result_cb(VkResult err)
{
	if (err != VK_SUCCESS) {
		fprintf(stderr, "(ee) %s\n", vk_strerror(err));
	}
}

static void cleanup_gui(struct context *ctx)
{
	VkResult err;
	vk_call(err, vkDeviceWaitIdle(ctx->vk_dev));

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	ImGui_ImplVulkanH_DestroyWindow(ctx->vk_instance, ctx->vk_dev,
	 &ctx->gui_win, ctx->vk_alloctor);
}

static inline bool is_minimized(ImDrawData *data)
{
	return (data->DisplaySize.x <= 0. || data->DisplaySize.y <= 0.);
}

static void clear_window(struct context *ctx)
{
	ImGui_ImplVulkanH_Window *win = &ctx->gui_win;
	ImVec4 clear_color = ImVec4(.1, .1, .1, 1.);

	win->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
	win->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
	win->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
	win->ClearValue.color.float32[3] = clear_color.w;
}

static void resize_window(struct context *ctx)
{
	ImGui_ImplVulkan_SetMinImageCount(ctx->image_count);
	ImGui_ImplVulkanH_CreateOrResizeWindow(ctx->vk_instance, ctx->vk_gpu,
	 ctx->vk_dev, &ctx->gui_win, ctx->vk_queue_family, ctx->vk_alloctor,
	 ctx->w, ctx->h, ctx->image_count);

	ctx->gui_win.FrameIndex = 0;
	ctx->rebuild_swapchain = false;
}

static void render_gui(struct context *ctx)
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (ctx->show_demo_window)
		ImGui::ShowDemoWindow(&ctx->show_demo_window);

	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();
	if (!is_minimized(draw_data)) {
		clear_window(ctx);
		render_frame(ctx, draw_data);
		present_gui(ctx);
	}
}

static bool init_font(struct context *ctx)
{
	VkResult err;
	ImGui_ImplVulkanH_Window *win = &ctx->gui_win;

	VkCommandPool cmdpool = win->Frames[win->FrameIndex].CommandPool;
	VkCommandBuffer cmdbuf = win->Frames[win->FrameIndex].CommandBuffer;

	vk_call(err, vkResetCommandPool(ctx->vk_dev, cmdpool, 0));
	if (err != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vk_call(err, vkBeginCommandBuffer(cmdbuf, &begin_info));
	if (err != VK_SUCCESS)
		return false;

	ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);

	VkSubmitInfo end_info = {};
	end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	end_info.commandBufferCount = 1;
	end_info.pCommandBuffers = &cmdbuf;

	vk_call(err, vkEndCommandBuffer(cmdbuf));
	if (err != VK_SUCCESS)
		return false;

	vk_call(err, vkQueueSubmit(ctx->vk_queue, 1, &end_info,
	 VK_NULL_HANDLE));
	if (err != VK_SUCCESS)
		return false;

	vk_call(err, vkDeviceWaitIdle(ctx->vk_dev));
	if (err != VK_SUCCESS)
		return false;

	ImGui_ImplVulkan_DestroyFontUploadObjects();
	return true;
}

static bool init_gui(struct context *ctx)
{
	ImGui_ImplVulkanH_Window *win = &ctx->gui_win;
	VkSurfaceFormatKHR format;
	const VkFormat formats[] = {
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_R8G8B8_UNORM,
	};
	const VkColorSpaceKHR color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };

	win->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(ctx->vk_gpu,
	 win->Surface, formats, ARRAY_SIZE(formats), color_space);
	win->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(ctx->vk_gpu,
	 win->Surface, &present_modes[0], ARRAY_SIZE(present_modes));

	ImGui_ImplVulkanH_CreateOrResizeWindow(ctx->vk_instance, ctx->vk_gpu,
	 ctx->vk_dev, win, ctx->vk_queue_family, ctx->vk_alloctor,
	 ctx->w, ctx->h, ctx->image_count);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	ImFontConfig font_cfg;
	font_cfg.SizePixels = ctx->font_size;
	io.Fonts->AddFontDefault(&font_cfg);

#ifdef ENABLE_KEYBOARD
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#endif
#ifdef ENABLE_GAMEPAD
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
#endif

#ifdef CLASSIC_UI
	ImGui::StyleColorsClassic();
#else
	ImGui::StyleColorsDark();
#endif

	ImGui::GetStyle().WindowTitleAlign = ImVec2(.5, .5);
	ImGui::GetStyle().WindowPadding = ImVec2(15, 15);
	ImGui::GetStyle().WindowBorderSize = 8.;
	ImGui::GetStyle().WindowRounding = 20.;

	ImGui_ImplGlfw_InitForVulkan(ctx->win, true);

	ImGui_ImplVulkan_InitInfo info = {};
	info.Instance = ctx->vk_instance;
	info.PhysicalDevice = ctx->vk_gpu;
	info.Device = ctx->vk_dev;
	info.QueueFamily = ctx->vk_queue_family;
	info.Queue = ctx->vk_queue;
	info.PipelineCache = ctx->vk_pipeline_cache;
	info.DescriptorPool = ctx->vk_descriptor_pool;
	info.Allocator = ctx->vk_alloctor;
	info.MinImageCount = ctx->image_count;
	info.ImageCount = win->ImageCount;
	info.CheckVkResultFn = vk_result_cb;

	ImGui_ImplVulkan_Init(&info, win->RenderPass);

	return true;
}

static void glfw_error_cb(int err, const char *str)
{
    fprintf(stderr, "(ee) GLFW error %d: %s\n", err, str);
}

static void glfw_key_cb(GLFWwindow *win, int key, int code, int act, int mods)
{
        if (key == GLFW_KEY_ESCAPE && act == GLFW_PRESS) {
                glfwSetWindowShouldClose(win, GLFW_TRUE);
        }
}

int main(int argc, const char *argv[])
{
	struct context *ctx = &ctx_;

	glfwSetErrorCallback(glfw_error_cb);

	if (!glfwInit())
		return 1;
	else if (!init_window(ctx))
		return 1;
	else if (!init_vulkan(ctx))
		return 1;
	else if (!init_surface(ctx))
		return 1;
	else if (!init_gui(ctx))
		return 1;
	else if (!init_font(ctx))
		return 1;

	glfwSetKeyCallback(ctx->win, glfw_key_cb);
	while (!glfwWindowShouldClose(ctx->win)) {
		glfwPollEvents();

		if (ctx->rebuild_swapchain) {
			glfwGetFramebufferSize(ctx->win,
			 &ctx->gui_win.Width, &ctx->gui_win.Height);

			if (ctx->gui_win.Width > 0 && ctx->gui_win.Height > 0)
				resize_window(ctx);
		}

		render_gui(ctx);
	}

	cleanup_gui(ctx);
	cleanup_vulkan(ctx);

	glfwDestroyWindow(ctx->win);
	glfwTerminate();

	return 0;
}
