#include "CurtainMeshRenderer.hpp"

#include "LogicalDevice.hpp"

#include <utility>
#include <ext/matrix_transform.hpp>

using namespace MFA;

namespace shared
{

	//-----------------------------------------------------------------------------

	CurtainMeshRenderer::CurtainMeshRenderer(
		std::shared_ptr<Pipeline> colorPipeline,
		std::shared_ptr<Pipeline> wireFramePipeline, 
		std::vector<glm::vec3> surfacePoints,
		std::vector<glm::vec3> surfaceNormals,
		float const curtainHeight
	)
		: _surfacePoints(std::move(surfacePoints))
		, _surfaceNormals(std::move(surfaceNormals))
		, _curtainHeight(curtainHeight)
		, _colorPipeline(std::move(colorPipeline))
		, _wireFramePipeline(std::move(wireFramePipeline))
	{
		UpdateGeometry();
	}

	//-----------------------------------------------------------------------------

	void CurtainMeshRenderer::Render(
		RecordState& recordState, 
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
				.color = options.fillColor
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
				Pipeline::PushConstants{
					.model = glm::identity<glm::mat4>(),
					.color = options.wireframeColor
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

	//-----------------------------------------------------------------------------

	void CurtainMeshRenderer::UpdateGeometry(
		std::vector<glm::vec3> surfacePoints,
		std::vector<glm::vec3> surfaceNormals,
		float const curtainHeight
	)
	{
		// TODO: Sample points first
		_surfacePoints = std::move(surfacePoints);
		_surfaceNormals = std::move(surfaceNormals);
		_curtainHeight = curtainHeight;

		UpdateGeometry();
	}

	//-----------------------------------------------------------------------------

	std::vector<CurtainMeshRenderer::CollisionTriangle> const& CurtainMeshRenderer::GetCollisionTriangles() const
	{
		return _collisionTriangles;
	}

	//-----------------------------------------------------------------------------

	void CurtainMeshRenderer::UpdateGeometry()
	{
		UpdateCpuIndices();
		UpdateCpuVertices();
		UpdateCollisionTriangles();

		auto const maxFramePerFlight = MFA::LogicalDevice::Instance->GetMaxFramePerFlight();
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

	//-----------------------------------------------------------------------------

	void CurtainMeshRenderer::UpdateCpuVertices()
	{
		int const pointCount = static_cast<int>(_surfacePoints.size());
		_vertices.resize(pointCount * 2);
		for (int i = 0; i < pointCount; ++i)
		{
			_vertices[i].position = _surfacePoints[i];
			_vertices[i + pointCount].position = _surfacePoints[i] + _surfaceNormals[i] * _curtainHeight;
		}
	}

	//-----------------------------------------------------------------------------

	void CurtainMeshRenderer::UpdateCpuIndices()
	{
		int const pointCount = static_cast<int>(_surfacePoints.size());
		_indices.clear();
		_triangles.clear();
		for (int i = 0; i < pointCount - 1; ++i)
		{
			{// Triangle0
				int id0 = i;
				int id1 = i + 1;
				int id2 = i + pointCount;

				_indices.emplace_back(id0);
				_indices.emplace_back(id1);
				_indices.emplace_back(id2);

				_triangles.emplace_back(std::tuple{ id0, id1, id2 });
			}
			{// Triangle1
				int id0 = i + pointCount;
				int id1 = i + 1 + pointCount;
				int id2 = i + 1;

				_indices.emplace_back(id0);
				_indices.emplace_back(id1);
				_indices.emplace_back(id2);

				_triangles.emplace_back(std::tuple{ id0, id1, id2 });
			}
		}
	}

	//-----------------------------------------------------------------------------

	void CurtainMeshRenderer::UpdateCollisionTriangles()
	{
		_collisionTriangles.resize(_triangles.size());
		#pragma omp parallel for
		for (int i = 0; i < static_cast<int>(_triangles.size()); ++i)
		{
			auto [idx0, idx1, idx2] = _triangles[i];

			auto const& v0 = _vertices[idx0].position;
			auto const& v1 = _vertices[idx1].position;
			auto const& v2 = _vertices[idx2].position;

			Collision::UpdateCollisionTriangle(
				v0,
				v1,
				v2,
				_collisionTriangles[i]
			);
		}
	}

	//-----------------------------------------------------------------------------

	void CurtainMeshRenderer::UpdateVertexBuffer(RecordState const& recordState)
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

	//-----------------------------------------------------------------------------

	void CurtainMeshRenderer::UpdateIndexBuffer(RecordState const& recordState)
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

	//-----------------------------------------------------------------------------

}
