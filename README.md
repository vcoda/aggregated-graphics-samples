# aggregated-graphics-samples
A collection of exemplary graphics samples based on Magma and Vulkan API

### [Bump mapping](bumpmapping/)
<img src="./screenshots/bumpmapping.jpg" height="144x" align="left">

Bump mapping was first introduced by James F. Blinn in [Simulation of Wrinkled Surfaces](https://www.microsoft.com/en-us/research/publication/simulation-of-wrinkled-surfaces/) (*SIGGRAPH '78: Proceedings of the 5th annual conference on Computer graphics and interactive techniques, August 1978*). He presented a method of using a texturing function to perform a small perturbation of the surface normal before using it in the lighting calculation. Height field bump map corresponds to a one-component texture map discretely encoding the bivariate function (see [A Practical and Robust Bump-mapping Technique forToday’s GPUs](https://www.researchgate.net/publication/2519643_A_Practical_and_Robust_Bump-mapping_Technique_for_Today's_GPUs)). Because of this, the best way to store height map is to use **BC4** unsigned normalized format (former ATI1/3Dc+). Usually micro-normal is computed from height map in fragment shader using finite differences and only three texture samples, but this may result in aliasing. This demo uses [Sobel filter](https://en.wikipedia.org/wiki/Sobel_operator) with eight texture samples to approximate normal with better precision. After micro-normal is computed, it is transformed from texture space to object space using per-pixel TBN matrix, described in [Normal Mapping without Precomputed Tangents](http://www.thetenthplanet.de/archives/1180) (*ShaderX 5, Chapter 2.6, pp. 131 – 140*). This allows to get rid of computing tangents for each vertex. Finally, normal transformed from object space to view space using so-called [Normal Matrix](https://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/). Lighting is computed as dot product of the normal vector with light vector, reproducing **DOT3_RGB** [texture combiner operation](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_env_dot3.txt) of old fixed-function hardware.

### [Normal mapping](normalmapping/)
<img src="./screenshots/normalmapping.jpg" height="144x" align="left">

Normal mapping is the most common variation of bump mapping. Instead of computing micro-normal from height map, pre-computed normal map is used. This demo uses normal map stored in two-component **BC5** signed normalized format (former [ATI2/3Dc](https://en.wikipedia.org/wiki/3Dc)) because for normal is it enough to store only X and Y coordinates. Z coordinate of unit vector is reconstructed in fragment shader. Despite this, normal map required twice the VRAM space in comparison to one-component height map. Nevertheless, backed normals have superior quality and require only one texture sample in comparison to three or eight texture samples required for height map.

### [Displacement mapping](displacement-mapping/)
<img src="./screenshots/displacement-mapping.jpg" height="144x" align="left">

Displacement mapping was first introduced by Robert L. Cook in [Shade Trees](https://graphics.pixar.com/library/ShadeTrees/paper.pdf) (*Computer Graphics, vol. 18, no. 3, July 1984*). Displacement mapping is a method for adding small-scale detail to surfaces. Unlike bump mapping, which affects only the shading of surfaces, displacement mapping adjusts the positions of surface elements. This leads to effects not possible with bump mapping, such as surface features that occlude each other and non-polygonal silhouettes. The drawback of this method is that it requires a lot of additional geometry. Displacement map stored in **BC3** format (former DXT5): normal in RGB components and height in alpha. Vertex displacement performed in the vertex shader. Heightmap sampled in the vertex shader and normal map sampled in the fragment shader. The latter computes not just diffuse lighting, but Phong BRDF to highlight bumps and wrinkles. You can change the displacement amount to see how it affects torus shape.

### [Self-shadowing](self-shadowing/)
<img src="./screenshots/self-shadowing.jpg" height="144x" align="left">

Geomentry normal or micro-normal can create self-shadowing situations. There are situations where the perturbed normal is subject to illumination based the light direction. However, the point on the surface arguably should not receive illumination from the light because the unperturbed normal indicates the point is self-shadowed due to the surface’s large-scale geometry. Without extra level of clamping, bump-mapped surfaces can show illumination in regions of the surface that should be self-shadowed based on the vertex-specified geometry of the model. To account for self-shadowing due to either the perturbed or unperturbed surface normal, a special step function is introduced (see [A Practical and Robust Bump-mapping Technique for Today’s GPUs](https://www.researchgate.net/publication/2519643_A_Practical_and_Robust_Bump-mapping_Technique_for_Today's_GPUs), *2.4.1 Bumped Diffuse Self-shadowing*). Step function uses a steep linear ramp to transition between 0 and 1 with coefficient=1/8, which is effective at minimizing popping and winking artifacts. You can toggle self-shadowing on and off to see how step function affects lighting.

### [G-buffer](gbuffer/)
<img src="./screenshots/gbuffer.jpg" height="144x" align="left">

Geometric Buffers was first introduced by Saito and Takahashi in [Comprehensible Rendering of 3-D Shapes](https://www.cs.princeton.edu/courses/archive/fall00/cs597b/papers/saito90.pdf) (*Computer Graphics, vol. 24, no. 4, August 1990*). G-buffers preserve geometric properties of the surfaces such as depth or normal. This demo implements G-buffers as multi-attachment framebuffer that stores depth, normal, albedo and specular properties. Normals are encoded into RG16 floating-point format texture using Spheremap Transform (see [Compact Normal Storage for small G-Buffers](https://aras-p.info/texts/CompactNormalStorage.html)). Normals from normal map transformed from texture space to world space using per-pixel TBN matrix, described in [Normal Mapping without Precomputed Tangents](http://www.thetenthplanet.de/archives/1180) (*ShaderX 5, Chapter 2.6, pp. 131 – 140*).

### [Deferred shading](deferred-shading/)
<img src="./screenshots/deferred-shading.jpg" height="144x" align="left">

Deferred shading was proposed by Deering et al. in [The triangle processor and normal vector shader: a VLSI system for high performance graphics](https://dl.acm.org/doi/abs/10.1145/378456.378468) (*Computer Graphics, vol. 22, no. 4, August 1988*). It normally consists from two passes:

* Positions, normals and surface attributes like albedo and specular are written into G-buffer.
* Fragment shader computes lighting for each light source, reconstructing position and normal from G-buffer as well as surface parameters for BRDF function.

This implementation performs additional depth pre-pass, in order to achieve zero overdraw in G-buffer. Writing geometry attributes usually requires a lot of memory bandwidth, so it's important to write to G-buffer only once in every pixel. First, depth-only pass writes depth values into G-buffer with *less-equal* depth test enabled. Second, G-buffer pass is performed with depth test enabled as *equal*, but depth write disabled. In Vulkan, for each pass we have to create separate color/depth render passes in order to clear/write only particular framebuffer's attachment(s). To simplify code, deferred shading is performed for single point light source.

### [Shadow mapping](shadowmapping/)
<img src="./screenshots/shadowmapping.jpg" height="144x" align="left">

Shadow mapping was first introduced by Lance Williams in [Casting curved shadows on curved surfaces](http://cseweb.ucsd.edu/~ravir/274/15/papers/p270-williams.pdf), (*Computer Graphics, vol. 12, no. 3, August 1978*). First, shadow caster is rendered to off-screen with depth format. In second pass, shadow is created by testing whether a pixel is visible from the light source, by doing comparison of fragment's depth in light view space with depth stored in shadow map.
<br><br><br>

### [Percentage closer filtering](shadowmapping-pcf/)
<img src="./screenshots/shadowmapping-pcf.jpg" height="144px" align="left">

Percentage closer filtering of shadow map. The technique was first introduced by Reeves et al. in [Rendering Antialiased Shadows with Depth Maps](https://graphics.pixar.com/library/ShadowMaps/paper.pdf) (*Computer Graphics, vol. 21, no. 4, July 1987*). Unlike normal textures, shadow map textures cannot be prefiltered to remove aliasing. Instead, multiple depth comparisons are made per pixel and averaged together. I have implemented two sampling patterns: regular grid and Poisson sampling. This particular implementation uses *textureOffset()* function with constant offsets to optimize URB usage (see [Performance tuning applications for Intel GEN Graphics for Linux and SteamOS](http://media.steampowered.com/apps/steamdevdays/slides/gengraphics.pdf)). Due to hardware restrictions, texture samples have fixed integer offsets, which reduces anti-aliasing quality.

### [Poisson shadow filtering](shadowmapping-poisson/)
<img src="./screenshots/shadowmapping-poisson.jpg" height="140px" align="left">

Soft shadow rendering with high-quality filtering using [Poisson disk sampling](https://sighack.com/post/poisson-disk-sampling-bridsons-algorithm).
To overcome hardware limitations, *textureOffset()* function was replaced by *texture()*, which allows to use texture coordinates with arbitrary floating-point offsets. To get rid of pattern artefacts I have implemented jittered sampling. For each fragment *noise()* function generates pseudo-random value that is expanded in [0, 2π] range for an angle in radians to construct rotation matrix. Jittered PCF samples are computed by rotating Poisson disk for each pixel on the screen.
<br><br>

### [Stable Poisson shadow filtering](shadowmapping-poisson-stable/)
<img src="./screenshots/shadowmapping-poisson-stable.jpg" height="140px" align="left">

In previous implementation Poisson jittering depends on screen position of the fragment. As neighboring fragments have random noise values, they define different rotation matrices. This causes shadow flickering from the filter pattern when shadow or camera is moving. In this demo I use a jitter pattern which is stable in world space. This means the random jitter offset depends on the world space position of the shadowed pixel and not on the screen space position. You can toggle between screen space and world space techniques to see how shadow filtering changes.
<br><br>
