# aggregated-graphics-samples
A collection of exemplary graphics samples based on Magma and Vulkan API

### [Shadow mapping](shadowmapping/)
<img src="./screenshots/shadowmapping.jpg" height="144x" align="left">

Shadow mapping was first introduced by Lance Williams in [Casting curved shadows on curved surfaces](http://cseweb.ucsd.edu/~ravir/274/15/papers/p270-williams.pdf), (*Computer Graphics, vol. 12, no. 3, August 1978*). First, shadow caster is rendered to off-screen with depth format. In second pass, shadow is created by testing whether a pixel is visible from the light source, by doing comparison of fragment's depth in light view space with depth stored in shadow map.
<br><br><br>

### [Percentage closer filtering](shadowmapping-pcf/)
<img src="./screenshots/shadowmapping-pcf.jpg" height="144px" align="left">

Percentage closer filtering of shadow map. The technique was first introduced by Reeves et al. in [Rendering Antialiased Shadows with Depth Maps](https://graphics.pixar.com/library/ShadowMaps/paper.pdf) (*Computer Graphics, vol. 21, no. 4, July 1987*). Unlike normal textures, shadow map textures cannot be prefiltered to remove aliasing. Instead, multiple depth comparisons are made per pixel and averaged together. I have implemented two sampling patterns: regular grid and Poisson sampling. This particular implementation uses *textureOffset()* function with constant offsets to optimize URB usage (see [Performance tuning applications for Intel GEN Graphics for Linux and SteamOS](http://media.steampowered.com/apps/steamdevdays/slides/gengraphics.pdf)). Due to hardware restrictions, texture samples have fixed integer offsets, which reduces anti-aliasing quality.
