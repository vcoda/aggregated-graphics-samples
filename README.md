# aggregated-graphics-samples
A collection of exemplary graphics samples based on Magma and Vulkan API

### [Shadow mapping](shadowmapping/)
<img src="./screenshots/shadowmapping.jpg" height="144x" align="left">

Shadow mapping was first introduced by Lance Williams in [Casting curved shadows on curved surfaces](http://cseweb.ucsd.edu/~ravir/274/15/papers/p270-williams.pdf), (*Computer Graphics, vol. 12, no. 3, August 1978*). First, shadow caster is rendered to off-screen with depth format. In second pass, shadow is created by testing whether a pixel is visible from the light source, by doing comparison of fragment's depth in light view space with depth stored in shadow map.
<br><br><br>
