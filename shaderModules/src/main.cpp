#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib> // provides EXIT_FAILURE and EXIT_SUCCESS
#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <string>
#include <cstdint> // for uint32_t
#include <limits> // std::numeric_limits (std::numeric_limits::max() = 0xFFFFFFFFF highest possible 32 bit unsigened int)
#include <algorithm> // std::clamp (Make sure width is not smaller than min or larger than max)
#include <fstream>

// globals
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG // NDEBUG is a macro meaning "not debug"
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Proxy function to get the address of vkCreateDebugUtilsMessengerEXT function
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) 
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, 
    VkDebugUtilsMessengerEXT debugMessenger, 
    const VkAllocationCallbacks *pAllocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) 
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static std::vector<char> readFile(const std::string& filename) {
    /*
    Reads all of the bytes from the specified file and returns them in byte array
    managed by std::vector
    ate : start reading at the end of the file
    binary : Read the file as a binary(avoid text transformations)

    # We start reading from the end cuz we can determine the size of the file
      and allocate a buffer
    */
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize); // Create buffer
    std::cout << "\tBuffer size: " << fileSize << "\n";
    file.seekg(0); // Go to the beginning of the file
    file.read(buffer.data(), fileSize); // (startPtr, endPtr)
    file.close();

    return buffer;
}

class HelloTriangleApplication {
public:
    void run()  {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:

    // This struct stores the queue family indices we care about
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;

        std::optional<uint32_t> presentFamily; // presentation-capable family

        bool isComplete() {

            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities; // Max/min number of images in the swap chain. & Max/min width of the images
        std::vector<VkSurfaceFormatKHR> formats; // px format / color space
        std::vector<VkPresentModeKHR> presentModes; 
    };

    GLFWwindow *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // just a nullptr macro // GPU physical device
    VkDevice device; // Logical device
    
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSurfaceKHR surface;

    VkSwapchainKHR swapChain;

    // we can retrive the handles of the VkImage in this vector
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    // Image views 
    std::vector<VkImageView> swapChainImageViews;


private:
    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface(); // surface must be made after the creation of instance as it actualy influences the physiacal device setup
        pickPhysicalDevice();
        createLogicDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
    }

    // Althought the creation of VkSurfaceKHR object and its usage are platform agnostic, it's creation it'nt
        // This is because it's creation is dependent on the window system details (different to each OS(just the details))
        // GLFW handles this for us with the function glfwCreateWindowSurface function
    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Could not create a window Surface!");
        } 
    }

    void createLogicDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

/* This below implementation is commented out cuz you did this before bringing in graphics family queue
        // Prepares a request for a queue from that family
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value(); // determines which queue family we are gonna use
        queueCreateInfo.queueCount = 1; // we want one queue (we can select more but just 1 would suffice)
*/
        // Vulkan lets us assign priorities to queues to influence the scheduling of command buffer execution
        float queuePriority = 1.f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Specifies the set of device features that we will be using
        /*
        Right now you are enabling no optional features.
           # Examples of optional features:
           # geometry shaders
           # tessellation
           # sampler anisotropy
           # wide lines
           # multi viewport
           # fill modes
        */
        VkPhysicalDeviceFeatures deviceFeatures{};

        // Filling in the main VkDeviceCreateInfo structure
        // This main structure tells vulkan to create a logical device using this GPU, these queues, these features enabled etc.
        // Logical Device = Vulkan's interface to that GPU
        // This API object gives access to :
        /*
            # GPU queues
            # GPU memory allocators
            # Buffers
            # images
            # Pipelines
            # Descriptor sets
            # Submitting work
        */
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        // Enable required device extensions (swapchain)
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();


        if (enableValidationLayers) {
            // The following fileds of VkDeviceCreateInfo are ignored by up-to-date implementations.
            // But it is still a good idea to set them up anyway to bee compatible with older implementations
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        } else {
            std::cout << "\tLogical device made successfully!" << std::endl;
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    // This function finds queue families available on the GPU
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        // VkBool32 presentSupport = false;  <-- removed redundant outer variable

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // Query present support for this family (do it for every family)
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    // Determines if a GPU is suitable by checking queue families only
    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);
        
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails SwapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !SwapChainSupport.formats.empty() && !SwapChainSupport.presentModes.empty(); // Are both the vectors not empty?
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            //std::cout << "Erased Extension: " << extension.extensionName << std::endl;
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    // Selects a physical device (GPU) that supports Vulkan
    void pickPhysicalDevice() {

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("Could not find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Pick the first device that is suitable
        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        } else {
            std::cout << "\n\tFound a compatible GPU device!\n";
        }
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        for (auto& imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapChain, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        vkDestroyDevice(device, nullptr);

        // Comes before the instance destruction (destroys the glfw surface)
        vkDestroySurfaceKHR(instance, surface, nullptr);

        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

// =============== INSTANCE CREATION + DEBUG MESSENGER ====================

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello triangle";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "No engine";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create an instance!");
        } else {
            std::cout << "\n\tInstance created successfully!\n";
        }
    }

    // This function checks if all the requested validation layers are available
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    // Debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData) {

        std::cerr << "validation layer: " << callbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    // Debug utils setup
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
           // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | // turn this on to get general logs 
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    // Required instance extensions
    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    // Swap chain
    // Function to populate the struct
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    // Choosing the right settings for the swap chain
    // Surface mode
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // this loop loops through all the surface formats the GPU + window system supports and picks the one we actually want 
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    // Presentation mode
    VkPresentModeKHR chooseSwapPresentMode(std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // Swap Extent
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        // vulkan sets the width to uint32_MAX when the window system does not specify a fixed image size
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t> (width),
                static_cast<uint32_t> (height)
            };

            // These clamp the values between the set min and max bounds 
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // Aside from these properties, we also have to decide how many images we would like to have in the swap chain
        //uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
        // If we simply stick to this minimum means that we may sometimes have to wait on the driver to complete internal operations before
        // we can acquire another image to render to. So, we request one more image than the minimum
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        // The imageArrayLayers specifies the amount of layers each image consists of. This is always 1 unless you are developing a stereoscopic 3D application
        createInfo.imageArrayLayers = 1;
        // Specifies what kind of we'll use the images in the swap chain for
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices  = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // no transformation
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("\n\tCould not create SwapChain!");
        } else {
            std::cout << "\n\tSwap Chain creation successful!" << std::endl;
        }

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    // image view
    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i]; // i'th swap chain image
            // the below two fields specify how the data has to be interpreted
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // allows you to treat images as 1D texture, 2D texture, 3D textures and cube maps
            createInfo.format = swapChainImageFormat; 
            // The below components field allows you to swizzle the color channels around.
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // The below subresource range filed describes what the image's purpose is and which part of the image should be addressed
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image views!");
            }
        }
    }

    // Graphics pipeline
    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("../shaders/vert.spv");
        auto fragShaderCode = readFile("../shaders/frag.spv");

        /*
        Shader modules are just a thin wrapper around the shader bytecode that we've loaded from a file
        and the functions defined in it. The compilation and linking of the SPIR-V to machine code for execution
        by the GPU does not happen until the Graphics pipeline is created.
        # We can destroy our modules right after the creation of the pipeline as they would no longer be needed so we can just keep them as local variables
        */
       VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
       VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

       /*
       To actually use the shaders we will need to assign them to a specific stage through VkPipelineShaderStageCreateInfo struct as part 
       of thr actual pipeline creation process.
       */
      VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
      vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      // Specifies in which pipeline stage the shader is gonna be used.
      vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
      vertShaderStageInfo.module = vertShaderModule; // Specifies the shader module containing the code
      vertShaderStageInfo.pName = "main"; // Which function to invoke (entrypoint)
      vertShaderStageInfo.pSpecializationInfo = nullptr; // Allows you to specify values for shader constants

      VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
      fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      fragShaderStageInfo.module = fragShaderModule;
      fragShaderStageInfo.pName = "main";

      VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

       vkDestroyShaderModule(device, fragShaderModule, nullptr);
       vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        /*
        # Creating a shader module is simple, we only need to specify a pointer to the buffer with the
          bytecode and the length of it. This info is specified in a VkShaderModuleCreateInfo struct.
        # The one catch is that the size of the bytecode is specifed in bytes but the byecode pointer is a
          uint32_t pointer rather than a char pointer. So we perform a reinterpret cast
        # When doing cast like we we gotta make sure to ensure the data satisfies alignment requiremwnrs of uint32_t.
        # The data stored in std::vector by default ensures that the data satisfies with the worst case alignment requirements
        */
       VkShaderModuleCreateInfo createInfo{};
       createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
       createInfo.codeSize = code.size();
       createInfo.pCode = reinterpret_cast<const uint32_t*> (code.data());

       VkShaderModule shaderModule;
       if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
       }

       return shaderModule;
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
