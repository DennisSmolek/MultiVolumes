//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DXFramework.h"
#include "Core/XUSG.h"
#include "RayTracing/XUSGRayTracing.h"

class MultiRayCaster
{
public:
	MultiRayCaster(const XUSG::RayTracing::Device::sptr& device);
	virtual ~MultiRayCaster();

	bool Init(XUSG::RayTracing::CommandList* pCommandList, const XUSG::DescriptorTableCache::sptr& descriptorTableCache,
		XUSG::Format rtFormat, uint32_t gridSize, uint32_t lightGridSize, uint32_t numVolumes, uint32_t numVolumeSrcs,
		const XUSG::DepthStencil::uptr* depths, std::vector<XUSG::Resource::uptr>& uploaders,
		XUSG::RayTracing::GeometryBuffer* pGeometry);
	bool LoadVolumeData(XUSG::CommandList* pCommandList, uint32_t i,
		const wchar_t* fileName, std::vector<XUSG::Resource::uptr>& uploaders);
	bool SetDepthMaps(const XUSG::DepthStencil::uptr* depths);
	bool SetViewport(uint32_t width, uint32_t height);

	void InitVolumeData(const XUSG::CommandList* pCommandList, uint32_t i);
	void SetIrradiance(const XUSG::ShaderResource* pIrradiance);
	void SetMaxSamples(uint32_t maxRaySamples, uint32_t maxLightSamples);
	void SetVolumesWorld(float size, const DirectX::XMFLOAT3& center);
	void SetVolumeWorld(uint32_t i, float size, const DirectX::XMFLOAT3& pos);
	void SetLightMapWorld(float size, const DirectX::XMFLOAT3& pos);
	void SetLight(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& color, float intensity);
	void SetAmbient(const DirectX::XMFLOAT3& color, float intensity);
	void UpdateFrame(uint8_t frameIndex, DirectX::CXMMATRIX viewProj, const DirectX::XMFLOAT4X4& shadowVP, const DirectX::XMFLOAT3& eyePt);
	void Render(XUSG::CommandList* pCommandList, uint8_t frameIndex, bool updateLight);
	void RayMarchL(const XUSG::CommandList* pCommandList, uint8_t frameIndex);

	const XUSG::DescriptorTable& GetLightSRVTable() const;
	XUSG::Resource* GetLightMap() const;

	static const uint8_t FrameCount = 3;

protected:
	enum PipelineIndex : uint8_t
	{
		LOAD_VOLUME_DATA,
		INIT_VOLUME_DATA,
		VOLUME_CULL,
		RAY_MARCH_L,
		RAY_MARCH_V,
		CUBE_DEPTH_PEEL,
		RENDER_CUBE,
		RESOLVE_OIT,
		RAY_TRACING,

		NUM_PIPELINE
	};

	enum CommandLayoutIndex : uint8_t
	{
		DISPATCH_LAYOUT,
		DRAW_LAYOUT,

		NUM_COMMAND_LAYOUT
	};

	enum SrvTable : uint8_t
	{
		SRV_TABLE_FILE_SRC,
		SRV_TABLE_VOLUME_DESCS,
		SRV_TABLE_VOLUME,
		SRV_TABLE_VIS_VOLUMES,
		SRV_TABLE_LIGHT_MAP,
		SRV_TABLE_DEPTH,
		SRV_TABLE_SHADOW,
		SRV_TABLE_CUBE_MAP,
		SRV_TABLE_CUBE_DEPTH,
		SRV_TABLE_K_COLORS,
		SRV_TABLE_K_DEPTHS,
		SRV_TABLE_IRRADIANCE,

		NUM_SRV_TABLE
	};

	enum UavMipTable : uint8_t
	{
		UAV_TABLE_CULL,
		UAV_TABLE_CUBE_MAP,
		UAV_TABLE_CUBE_DEPTH,
		UAV_TABLE_LIGHT_MAP,
		UAV_TABLE_K_COLORS,
		UAV_TABLE_K_DEPTHS,

		NUM_UAV_TABLE
	};

	enum DepthIndex : uint8_t
	{
		DEPTH_MAP,
		SHADOW_MAP
	};

	bool createCubeIB(XUSG::CommandList* pCommandList, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createVolumeInfoBuffers(XUSG::CommandList* pCommandList, uint32_t numVolumes,
		uint32_t numVolumeSrcs, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createPipelineLayouts();
	bool createPipelines(XUSG::Format rtFormat);
	bool createCommandLayouts();
	bool createDescriptorTables();
	bool buildAccelerationStructures(const XUSG::RayTracing::CommandList* pCommandList,
		XUSG::RayTracing::GeometryBuffer* pGeometries);
	bool buildShaderTables();

	void cullVolumes(const XUSG::CommandList* pCommandList, uint8_t frameIndex);
	void rayMarchV(XUSG::CommandList* pCommandList, uint8_t frameIndex);
	void cubeDepthPeel(XUSG::CommandList* pCommandList, uint8_t frameIndex);
	void renderCube(XUSG::CommandList* pCommandList, uint8_t frameIndex);
	void resolveOIT(const XUSG::CommandList* pCommandList, uint8_t frameIndex);

	XUSG::RayTracing::Device::sptr m_device;

	XUSG::RayTracing::BottomLevelAS::uptr m_bottomLevelAS;
	XUSG::RayTracing::TopLevelAS::uptr m_topLevelAS;

	XUSG::VertexBuffer::uptr	m_vertexBuffer;
	XUSG::IndexBuffer::uptr		m_indexBuffer;

	// Shader tables
	static const wchar_t* HitGroupName;
	static const wchar_t* RaygenShaderName;
	static const wchar_t* ClosestHitShaderName;
	static const wchar_t* MissShaderName;
	XUSG::RayTracing::ShaderTable::uptr	m_missShaderTable;
	XUSG::RayTracing::ShaderTable::uptr	m_hitGroupShaderTable;
	XUSG::RayTracing::ShaderTable::uptr	m_rayGenShaderTable;

	XUSG::ShaderPool::uptr				m_shaderPool;
	XUSG::RayTracing::PipelineCache::uptr m_rayTracingPipelineCache;
	XUSG::Graphics::PipelineCache::uptr	m_graphicsPipelineCache;
	XUSG::Compute::PipelineCache::uptr	m_computePipelineCache;
	XUSG::PipelineLayoutCache::uptr		m_pipelineLayoutCache;
	XUSG::DescriptorTableCache::sptr	m_descriptorTableCache;

	XUSG::PipelineLayout	m_pipelineLayouts[NUM_PIPELINE];
	XUSG::Pipeline			m_pipelines[NUM_PIPELINE];
	XUSG::CommandLayout::uptr m_commandLayouts[NUM_COMMAND_LAYOUT];

	std::vector<XUSG::DescriptorTable> m_uavInitTables;
	XUSG::DescriptorTable	m_cbvTables[FrameCount];
	XUSG::DescriptorTable	m_cbvSrvTables[FrameCount];
	XUSG::DescriptorTable	m_uavTables[NUM_UAV_TABLE];
	XUSG::DescriptorTable	m_srvTables[NUM_SRV_TABLE];
	XUSG::DescriptorTable	m_samplerTable;

	std::vector<XUSG::ShaderResource::sptr>	m_fileSrcs;
	std::vector<XUSG::Texture3D::uptr>	m_volumes;
	std::vector<XUSG::Texture2D::uptr>	m_cubeMaps;
	std::vector<XUSG::Texture2D::uptr>	m_cubeDepths;
	XUSG::Texture3D::uptr		m_lightMap;
	XUSG::Texture2D::uptr		m_kDepths;
	XUSG::Texture2D::uptr		m_kColors;
	XUSG::ConstantBuffer::uptr	m_cbPerFrame;
	XUSG::StructuredBuffer::uptr m_perObject;
	XUSG::StructuredBuffer::uptr m_volumeDescs;
	XUSG::StructuredBuffer::uptr m_visibleVolumes;
	XUSG::StructuredBuffer::uptr m_counterReset;
	XUSG::RawBuffer::uptr		m_volumeDispatchArg;
	XUSG::RawBuffer::uptr		m_volumeDrawArg;
	XUSG::RawBuffer::sptr		m_visibleVolumeCounter;
	XUSG::TypedBuffer::uptr		m_volumeVis;

	const XUSG::DepthStencil::uptr* m_pDepths;
	const XUSG::ShaderResource* m_pIrradiance;

	XUSG::Resource::uptr		m_scratch;
	XUSG::Resource::uptr		m_instances;

	uint32_t				m_gridSize;
	uint32_t				m_lightGridSize;
	uint32_t				m_maxRaySamples;
	uint32_t				m_maxLightSamples;

	DirectX::XMFLOAT3		m_lightPt;
	DirectX::XMFLOAT4		m_lightColor;
	DirectX::XMFLOAT4		m_ambient;
	DirectX::XMFLOAT3X4		m_lightMapWorld;
	std::vector<DirectX::XMFLOAT3X4> m_volumeWorlds;
};
