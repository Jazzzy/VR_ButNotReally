C:\dep\VulkanSDK\1.0.65.1\Bin\glslangValidator.exe -V triangle.vert -o ..\..\res\shaders\triangle_vert.spv
C:\dev\VulkanRenderer\BinaryToCpp\bin\x64\Release\BinaryToCpp.exe ..\..\res\shaders\triangle_vert.spv triangle_vert > triangle_vert.hpp

C:\dep\VulkanSDK\1.0.65.1\Bin\glslangValidator.exe -V triangle.frag -o ..\..\res\shaders\triangle_frag.spv
C:\dev\VulkanRenderer\BinaryToCpp\bin\x64\Release\BinaryToCpp.exe ..\..\res\shaders\triangle_frag.spv triangle_frag > triangle_frag.hpp
