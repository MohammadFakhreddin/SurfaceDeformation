#include "SurfaceMeshRenderer.hpp"

#include <ext/matrix_transform.hpp>

#include "BedrockAssert.hpp"
#include "BedrockMemory.hpp"
#include "LogicalDevice.hpp"
#include "RenderBackend.hpp"

using namespace MFA;
using namespace geometrycentral;

//------------------------------------------------------------

shared::SurfaceMeshRenderer::SurfaceMeshRenderer(
	std::shared_ptr<Pipeline> colorPipeline,
	std::shared_ptr<Pipeline> wireFramePipeline,
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<Geometry> geometry,
	glm::vec4 const& fillColor,
	glm::vec4 const& wireFrameColor
)
	: _colorPipeline(std::move(colorPipeline))
	, _wireFramePipeline(std::move(wireFramePipeline))
	, _mesh(std::move(mesh))
	, _geometry(std::move(geometry))
	, _fillColor(fillColor)
	, _wireFrameColor(wireFrameColor)
{
	UpdateGeometry();
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateGeometry()
{
	UpdateCpuVertices();
	UpdateCpuIndices();

	auto const maxFramePerFlight = LogicalDevice::Instance->GetMaxFramePerFlight();
	_bufferDirtyCounter = static_cast<int>(maxFramePerFlight);
	
	_vertexBuffers.resize(maxFramePerFlight);
	_vertexBufferSizes.resize(maxFramePerFlight);

	_indexBuffers.resize(maxFramePerFlight);
	_indexBufferSizes.resize(maxFramePerFlight);
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::Render(
	RecordState & recordState, 
	RenderOptions const& options
)
{
	if (_bufferDirtyCounter > 0)
	{
		UpdateVertexBuffer(recordState);
		UpdateIndexBuffer(recordState);
		--_bufferDirtyCounter;
	}

	// Color shading
	_colorPipeline->BindPipeline(recordState);
	
	RB::BindIndexBuffer(
		recordState,
		*_indexBuffers[recordState.frameIndex],
		0,
		VK_INDEX_TYPE_UINT32
	);

	RB::BindVertexBuffer(
		recordState,
		*_vertexBuffers[recordState.frameIndex],
		0,
		0
	);

	_colorPipeline->SetPushConstants(
		recordState,
		ColorPipeline::PushConstants{
			.model = glm::identity<glm::mat4>(),
			.color = _fillColor
		}
	);

	RB::DrawIndexed(
		recordState,
		static_cast<int>(_indices.size()),
		1,
		0
	);

	// Wire frame shading
	if (options.useWireframe == true)
	{
		_wireFramePipeline->BindPipeline(recordState);

		RB::BindIndexBuffer(
			recordState,
			*_indexBuffers[recordState.frameIndex],
			0,
			VK_INDEX_TYPE_UINT32
		);

		RB::BindVertexBuffer(
			recordState,
			*_vertexBuffers[recordState.frameIndex],
			0,
			0
		);

		_wireFramePipeline->SetPushConstants(
			recordState,
			ColorPipeline::PushConstants{
				.model = glm::identity<glm::mat4>()
			}
		);

		_colorPipeline->SetPushConstants(
			recordState,
			ColorPipeline::PushConstants{
				.model = glm::identity<glm::mat4>(),
				.color = _wireFrameColor
			}
		);

		RB::DrawIndexed(
			recordState,
			static_cast<int>(_indices.size()),
			1,
			0
		);
	}

}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateCpuVertices()
{
	_vertices.clear();
	auto const positions = _geometry->vertexPositions;
	for (auto const& mV : positions.toVector())
	{
		_vertices.emplace_back(glm::vec3 {mV.x, mV.y, mV.z});
	}
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateCpuIndices()
{
	_indices.clear();
	auto const faceVertexList = _mesh->getFaceVertexList();
	for (auto const & faceVertices : faceVertexList)
	{
		if (faceVertices.size() == 3)
		{
			_indices.emplace_back(faceVertices[0]);
			_indices.emplace_back(faceVertices[1]);
			_indices.emplace_back(faceVertices[2]);
		}
		else if (faceVertices.size() == 4)
		{
			// Triangle0
			_indices.emplace_back(faceVertices[0]);
			_indices.emplace_back(faceVertices[1]);
			_indices.emplace_back(faceVertices[2]);
			// Triangle1
			_indices.emplace_back(faceVertices[3]);
			_indices.emplace_back(faceVertices[2]);
			_indices.emplace_back(faceVertices[0]);
		}
		else
		{
			MFA_ASSERT(false);
		}
	}
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateVertexBuffer(RecordState const& recordState)
{
	auto const* device = LogicalDevice::Instance;
	auto* vkDevice = device->GetVkDevice();
	auto* physicalDevice = device->GetPhysicalDevice();

	Alias const alias{ _vertices.data(), _vertices.size() };

	auto const vertexBufferSize = sizeof(Pipeline::Vertex) * _vertices.size();

	if (vertexBufferSize > _vertexBufferSizes[recordState.frameIndex])
	{
		_vertexBufferSizes[recordState.frameIndex] = vertexBufferSize;
		_vertexBuffers[recordState.frameIndex] = RB::CreateBuffer(
			vkDevice,
			physicalDevice,
			vertexBufferSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		);
	}

	RB::UpdateHostVisibleBuffer(
		vkDevice,
		*_vertexBuffers[recordState.frameIndex],
		alias
	);
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateIndexBuffer(RecordState const& recordState)
{
	auto const* device = LogicalDevice::Instance;
	auto* vkDevice = device->GetVkDevice();
	auto* vkPhysicalDevice = device->GetPhysicalDevice();

	Alias const alias{ _indices.data(), _indices.size() };

	auto const indexBufferSize = sizeof(Index) * _indices.size();

	if (indexBufferSize > _indexBufferSizes[recordState.frameIndex])
	{
		_indexBufferSizes[recordState.frameIndex] = indexBufferSize;
		_indexBuffers[recordState.frameIndex] = RB::CreateBuffer(
			vkDevice,
			vkPhysicalDevice,
			indexBufferSize,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		);
	}

	RB::UpdateHostVisibleBuffer(
		vkDevice,
		*_indexBuffers[recordState.frameIndex],
		alias
	);
}

//------------------------------------------------------------

