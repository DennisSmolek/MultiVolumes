//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RayMarch.hlsli"

//--------------------------------------------------------------------------------------
// Buffers and textures
//--------------------------------------------------------------------------------------
RWTexture3D<float3> g_rwLightMap;

StructuredBuffer<PerObject>		g_roPerObject	: register (t0);
StructuredBuffer<VolumeDesc>	g_roVolumes		: register (t1);

//--------------------------------------------------------------------------------------
// Compute Shader
//--------------------------------------------------------------------------------------
[numthreads(4, 4, 4)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float3 gridSize;
	g_rwLightMap.GetDimensions(gridSize.x, gridSize.y, gridSize.z);

	uint2 structInfo;
	g_roVolumes.GetDimensions(structInfo.x, structInfo.y);

	float4 rayOrigin;
	rayOrigin.xyz = (DTid + 0.5) / gridSize * 2.0 - 1.0;
	rayOrigin.w = 1.0;

	rayOrigin.xyz = mul(rayOrigin, g_lightMapWorld);	// Light-map space to world space

#ifdef _HAS_SHADOW_MAP_
	min16float shadow = ShadowTest(rayOrigin.xyz, g_txDepth);
#else
	min16float shadow = 1.0;
#endif

#ifdef _HAS_LIGHT_PROBE_
	min16float ao = 1.0;
	float3 irradiance = 0.0;
#endif

	// Find the volume of which the current position is nonempty
	bool hasDensity = false;
	float3 uvw = 0.0;
	PerObject perObject;
	VolumeDesc volume;
	for (uint n = 0; n < structInfo.x; ++n)
	{
		perObject = g_roPerObject[n];
		const float3 localRayOrigin = mul(rayOrigin, perObject.WorldI);	// World space to volume space

		if (all(abs(localRayOrigin) <= 1.0))
		{
			volume = g_roVolumes[n];
			uvw = LocalToTex3DSpace(localRayOrigin);
			volume.VolTexId = WaveReadLaneFirst(volume.VolTexId);

			const min16float density = GetSample(volume.VolTexId, uvw).w;
			hasDensity = density >= ZERO_THRESHOLD;

			if (hasDensity) break;
		}
	}

	if (hasDensity)
	{
		float3 aoRayDir = 0.0;
#ifdef _HAS_LIGHT_PROBE_
		if (g_hasLightProbes)
		{
			aoRayDir = -GetDensityGradient(volume.VolTexId, uvw);
			irradiance = GetIrradiance(mul(aoRayDir, (float3x3)perObject.World));
			aoRayDir = normalize(aoRayDir);
		}
#endif

		for (n = 0; n < structInfo.x; ++n)
		{
			const PerObject perObject = g_roPerObject[n];
			float3 localRayOrigin = mul(rayOrigin, perObject.WorldI);	// World space to volume space

			if (shadow >= ZERO_THRESHOLD)
			{
#ifdef _POINT_LIGHT_
				const float3 localSpaceLightPt = mul(g_lightPos, perObject.WorldI);
				const float3 rayDir = normalize(localSpaceLightPt - localRayOrigin);
#else
				const float3 localSpaceLightPt = mul(g_lightPos.xyz, (float3x3)perObject.WorldI);
				const float3 rayDir = normalize(localSpaceLightPt);
#endif
				// Transmittance
				if (!ComputeRayOrigin(localRayOrigin, rayDir)) continue;
				VolumeDesc volume = g_roVolumes[n];
				volume.VolTexId = WaveReadLaneFirst(volume.VolTexId);

				float t = g_stepScale;
				for (uint i = 0; i < g_numSamples; ++i)
				{
					const float3 pos = localRayOrigin + rayDir * t;
					if (any(abs(pos) > 1.0)) break;
					const float3 uvw = LocalToTex3DSpace(pos);

					// Get a sample along light ray
					const min16float density = GetSample(volume.VolTexId, uvw).w;

					// Attenuate ray-throughput along light direction
					shadow *= 1.0 - GetOpacity(density, g_stepScale);
					if (shadow < ZERO_THRESHOLD) break;

					// Update position along light ray
					t += g_stepScale;
				}
			}

#ifdef _HAS_LIGHT_PROBE_
			if (g_hasLightProbes)
			{
				float t = g_stepScale;
				for (uint i = 0; i < g_numSamples; ++i)
				{
					const float3 pos = localRayOrigin.xyz + aoRayDir * t;
					if (any(abs(pos) > 1.0)) break;
					const float3 uvw = LocalToTex3DSpace(pos);

					// Get a sample along light ray
					const min16float density = GetSample(volume.VolTexId, uvw).w;

					// Attenuate ray-throughput along light direction
					ao *= 1.0 - GetOpacity(density, g_stepScale);
					if (ao < ZERO_THRESHOLD) break;

					// Update position along light ray
					t += g_stepScale;
				}
			}
#endif
		}
	}

	const min16float3 lightColor = min16float3(g_lightColor.xyz * g_lightColor.w);
	min16float3 ambient = min16float3(g_ambient.xyz * g_ambient.w);

#ifdef _HAS_LIGHT_PROBE_
	ambient = g_hasLightProbes ? min16float3(irradiance) * ao : ambient;
#endif

	g_rwLightMap[DTid] = lightColor * shadow + ambient;
}
