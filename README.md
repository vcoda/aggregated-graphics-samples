# aggregated-graphics-samples
A collection of exemplary graphics samples based on Magma and Vulkan API

### [Normal mapping](normalmapping/)
<img src="./screenshots/normalmapping.jpg" height="144x" align="left">

Normal mapping was first introduced by James F. Blinn in [Simulation of Wrinkled Surfaces](https://www.microsoft.com/en-us/research/publication/simulation-of-wrinkled-surfaces/) (*SIGGRAPH '78: Proceedings of the 5th annual conference on Computer graphics and interactive techniques, August 1978*). A common use of this technique is to greatly enhance the appearance and details of a low polygon model by generating a normal map from a high polygon model or height map. This demo uses normal map stored in BC5 signed normalized format (former [3Dc](https://en.wikipedia.org/wiki/3Dc)) instead of BC2/BC3 (former DXT3/DXT5) because of higher compression ratio. Z component of normal vector is reconstructed in fragment shader. Next, normal transformed from texture space to object space using per-pixel TBN matrix, described in [Normal Mapping without Precomputed Tangents](http://www.thetenthplanet.de/archives/1180) (*ShaderX 5, Chapter 2.6, pp. 131 – 140*). This allows to get rid of precomputing tangents for each vertex. Finally, normal transformed from object space to view space using so-called [Normal Matrix](https://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/). Lighting is computed as dot product of the normal vector with light vector, reproducing old **GL_DOT3_RGB** texture combiner operation of [fixed-function hardware](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_env_dot3.txt).

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
