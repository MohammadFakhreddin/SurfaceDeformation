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
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<Geometry> geometry
)
	: _colorPipeline(std::move(colorPipeline))
	, _wireFramePipeline(std::move(wireFramePipeline))
	, _mesh(std::move(mesh))
	, _geometry(std::move(geometry))
{
	UpdateGeometry();
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateGeometry()
{
	UpdateCpuIndices();
	UpdateCpuVertices();
	UpdateCollisionTriangles();

	auto const maxFramePerFlight = LogicalDevice::Instance->GetMaxFramePerFlight();
	_bufferDirtyCounter = static_cast<int>(maxFramePerFlight);

	_vertexBuffers.resize(maxFramePerFlight);
	_vertexBufferSizes.resize(maxFramePerFlight);

	_indexBuffers.resize(maxFramePerFlight);
	_indexBufferSizes.resize(maxFramePerFlight);
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
				.color = instance.fillColor
			}
		);

		RB::DrawIndexed(
			recordState,
			static_cast<int>(_indices.size()),
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
					.color = instance.wireFrameColor
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

}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateGeometry(
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<Geometry> geometry
)
{
	_mesh = std::move(mesh);
	_geometry = std::move(geometry);
	UpdateGeometry();
}

//------------------------------------------------------------

std::vector<CollisionTriangle> shared::SurfaceMeshRenderer::GetCollisionTriangles(glm::mat4 const& model) const
{
	std::vector<CollisionTriangle> result = _collisionTriangles;

	#pragma omp parallel for
	for (int i = 0; i < static_cast<int>(result.size()); ++i)
	{
		auto& triangle = result[i];

		triangle.normal = glm::normalize(model * glm::vec4{ triangle.normal, 0.0f });
		triangle.center = model * glm::vec4{ triangle.center, 1.0f };

		for (auto& edgeNormal : triangle.edgeNormals)
		{
			edgeNormal = glm::normalize(model * glm::vec4{ edgeNormal, 0.0f });
		}
		for (auto& edgeVertex : triangle.edgeVertices)
		{
			edgeVertex = model * glm::vec4{ edgeVertex, 1.0f };
		}
	}

	return result;
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateCpuVertices()
{
	_vertices.clear();
	auto const positions = _geometry->vertexPositions;
	for (auto const& mV : positions.toVector())
	{
		_vertices.emplace_back(glm::vec3{ mV.x, mV.y, mV.z });
	}
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateCpuIndices()
{
	_indices.clear();
	_vertexNeighbourTriangles.clear();
	_triangles.clear();

	auto const faceVertexList = _mesh->getFaceVertexList();
	for (auto const& faceVertices : faceVertexList)
	{
		if (faceVertices.size() == 3)
		{
			int idx0 = faceVertices[0];
			int idx1 = faceVertices[1];
			int idx2 = faceVertices[2];

			_indices.emplace_back(idx0);
			_indices.emplace_back(idx1);
			_indices.emplace_back(idx2);

			_triangles.emplace_back(std::tuple{ idx0, idx1, idx2 });

			_vertexNeighbourTriangles[idx0].emplace_back(_triangles.size() - 1);
			_vertexNeighbourTriangles[idx1].emplace_back(_triangles.size() - 1);
			_vertexNeighbourTriangles[idx2].emplace_back(_triangles.size() - 1);
		}
		else if (faceVertices.size() == 4)
		{
			{// Triangle0
				int idx0 = faceVertices[0];
				int idx1 = faceVertices[1];
				int idx2 = faceVertices[2];

				_indices.emplace_back(idx0);
				_indices.emplace_back(idx1);
				_indices.emplace_back(idx2);

				_triangles.emplace_back(std::tuple{ idx0, idx1, idx2 });

				_vertexNeighbourTriangles[idx0].emplace_back(_triangles.size() - 1);
				_vertexNeighbourTriangles[idx1].emplace_back(_triangles.size() - 1);
				_vertexNeighbourTriangles[idx2].emplace_back(_triangles.size() - 1);
			}
			{// Triangle1
				int idx0 = faceVertices[2];
				int idx1 = faceVertices[3];
				int idx2 = faceVertices[0];

				_indices.emplace_back(idx0);
				_indices.emplace_back(idx1);
				_indices.emplace_back(idx2);

				_triangles.emplace_back(std::tuple{ idx0, idx1, idx2 });

				_vertexNeighbourTriangles[idx0].emplace_back(_triangles.size() - 1);
				_vertexNeighbourTriangles[idx1].emplace_back(_triangles.size() - 1);
				_vertexNeighbourTriangles[idx2].emplace_back(_triangles.size() - 1);
			}
		}
		else
		{
			MFA_ASSERT(false);
		}
	}
}

//------------------------------------------------------------

void shared::SurfaceMeshRenderer::UpdateCollisionTriangles()
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

