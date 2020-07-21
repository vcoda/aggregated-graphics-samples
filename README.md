# aggregated-graphics-samples
A collection of exemplary graphics samples based on Magma and Vulkan API

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
