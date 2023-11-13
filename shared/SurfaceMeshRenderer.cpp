#include "SurfaceMeshRenderer.hpp"

#include "BedrockAssert.hpp"
#include "BedrockMemory.hpp"
#include "LogicalDevice.hpp"
#include "RenderBackend.hpp"

#include <ext/matrix_transform.hpp>

using namespace MFA;
using namespace geometrycentral;

//------------------------------------------------------------

shared::SurfaceMeshRenderer::SurfaceMeshRenderer(
	std::shared_ptr<Pipeline> colorPipeline,
	std::shared_ptr<Pipeline> wireFramePipeline,
	std::shared_ptr<SurfaceMesh> surfaceMesh
)
	: _colorPipeline(std::move(colorPipeline))
	, _wireFramePipeline(std::move(wireFramePipeline))
	, _surfaceMesh(std::move(surfaceMesh))
{
	UpdateGeometry();
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateGeometry()
{
	auto const maxFramePerFlight = LogicalDevice::Instance->GetMaxFramePerFlight();
	_bufferDirtyCounter = static_cast<int>(maxFramePerFlight);

	if (_vertexBuffers.size() != maxFramePerFlight)
	{
		_vertexBuffers.resize(maxFramePerFlight);
	}
	if (_vertexBufferSizes.size() != maxFramePerFlight)
	{
		_vertexBufferSizes.resize(maxFramePerFlight);
	}

	if (_indexBuffers.size() != maxFramePerFlight)
	{
		_indexBuffers.resize(maxFramePerFlight);
	}
	if (_indexBufferSizes.size() != maxFramePerFlight)
	{
		_indexBufferSizes.resize(maxFramePerFlight);
	}
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::Render(
	RecordState& recordState,
	RenderOptions const& options,
	std::vector<InstanceOptions> const& instances
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

	for (auto const& instance : instances)
	{
		_colorPipeline->SetPushConstants(
			recordState,
			ColorPipeline::PushConstants{
				.model = instance.fillModel,
				.materialColor = instance.fillColor,
				.lightPosition = instance.lightPosition,
				.lightColor = instance.lightColor,
			}
		);

		RB::DrawIndexed(
			recordState,
			static_cast<int>(_surfaceMesh->GetIndices().size()),
			1,
			0
		);
	}

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

		for (auto const& instance : instances)
		{
			_wireFramePipeline->SetPushConstants(
				recordState,
				Pipeline::PushConstants{
					.model = instance.wireFrameModel,
					.materialColor = instance.wireFrameColor,
					.lightPosition = instance.lightPosition,
					.lightColor = instance.lightColor
				}
			);

			RB::DrawIndexed(
				recordState,
				static_cast<int>(_surfaceMesh->GetIndices().size()),
				1,
				0
			);
		}
	}

}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateGeometry(std::shared_ptr<SurfaceMesh> surfaceMesh)
{
	_surfaceMesh = std::move(surfaceMesh);
	UpdateGeometry();
}

//------------------------------------------------------------

std::vector<CollisionTriangle> shared::SurfaceMeshRenderer::GetCollisionTriangles(glm::mat4 const& model) const
{
	return _surfaceMesh->GetCollisionTriangles(model);
}

//------------------------------------------------------------

bool shared::SurfaceMeshRenderer::GetVertexIndices(int const triangleIdx, std::tuple<int, int, int>& outVIds) const
{
	return _surfaceMesh->GetVertexIndices(triangleIdx, outVIds);
}

//------------------------------------------------------------

bool shared::SurfaceMeshRenderer::GetVertexNeighbors(int const vertexIdx, std::set<int>& outVIds) const
{
	return _surfaceMesh->GetVertexNeighbors(vertexIdx, outVIds);
}

//------------------------------------------------------------

bool shared::SurfaceMeshRenderer::GetVertexPosition(int const vertexIdx, glm::vec3 & outPosition) const
{
	return _surfaceMesh->GetVertexPosition(vertexIdx, outPosition);
}

//------------------------------------------------------------

int shared::SurfaceMeshRenderer::GetVertexIdx(glm::vec3 const& position) const
{
	return _surfaceMesh->GetVertexIdx(position);
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateVertexBuffer(RecordState const& recordState)
{
	auto const* device = LogicalDevice::Instance;
	auto* vkDevice = device->GetVkDevice();
	auto* physicalDevice = device->GetPhysicalDevice();

	auto & vertices = _surfaceMesh->GetVertices();
	Alias const alias{ vertices.data(), vertices.size() };

	auto const vertexBufferSize = sizeof(Pipeline::Vertex) * vertices.size();

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

	auto & indices = _surfaceMesh->GetIndices();
	Alias const alias{ indices.data(), indices.size() };

	auto const indexBufferSize = sizeof(Index) * indices.size();

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

