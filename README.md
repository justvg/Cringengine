# Cringengine

Vulkan renderer with the goal to write something that looks like "GPU driven rendering". It implements GPU frustum and occlusion culling, LOD selection and uses vkCmdDrawIndexedIndirectCount to render the entire scene using one draw call.

# Building

To build and run this project you will need Visual Studio 2019 and Vulkan SDK. Just clone the repository and open Cringengine.sln.

# Inspiration

The renderer is inspired by Niagara renderer that was written on stream on Youtube. https://github.com/zeux/niagara