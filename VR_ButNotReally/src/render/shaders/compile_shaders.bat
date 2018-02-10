echo.
echo Compiling and embedding shaders...


@echo off
	set types=vert tesc tese geom frag conf
	(for %%t in (%types%) do (  
		for /R "./" %%f in (*.%%t) do (

			C:\dep\VulkanSDK\1.0.65.1\Bin\glslangValidator.exe -V %%f -o %%~nf_%%t.spv
			C:\dev\VulkanRenderer\BinaryToCpp\bin\x64\Release\BinaryToCpp.exe %%~nf_%%t.spv %%~nf_%%t > %%~nf_%%t.hpp
			del %%~nf_%%t.spv

			echo "Compiled and embedded %%t shader: %%~nf"
		)
	))


echo Finished compilation and embedding of shaders
echo.
echo.