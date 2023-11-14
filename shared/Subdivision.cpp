#include "Subdivision.hpp"

#include "SurfaceMeshRenderer.hpp"

namespace shared
{

	using namespace geometrycentral;
	using namespace geometrycentral::surface;

	//--------------------------------------------------------------------------------------------------------

	std::unique_ptr<ContributionMap> CatmullClarkSubdivide(
		ManifoldSurfaceMesh& mesh,
		VertexPositionGeometry& geo
	)
	{
		Eigen::Matrix<Vector3, Eigen::Dynamic, 1> const prevLvlVs = geo.vertexPositions.toVector();

		std::unordered_map<Face, std::unordered_map<Vertex, float>> vToFContrib{};
		std::unordered_map<Edge, std::unordered_map<Vertex, float>> vToEContrib{};
		std::unordered_map<Vertex, std::unordered_map<Vertex, float>> vToVContrib{};

		VertexData<Vector3> oldPositions = geo.inputVertexPositions;

		// Compute new positions for original vertices
		VertexData<Vector3> newPositions(mesh);

		FaceData<Vector3> splitFacePositions(mesh);
		for (Face f : mesh.faces()) {
			double D = (double)f.degree();
			splitFacePositions[f] = Vector3::zero();
			for (Vertex v : f.adjacentVertices()) {
				splitFacePositions[f] += geo.inputVertexPositions[v] / D;
			}

			std::unordered_map<Vertex, float> map{};
			for (Vertex vertex : f.adjacentVertices()) 
			{
				float value = 1.0f / D;
				auto findRes = map.find(vertex);
				if (findRes != map.end())
				{
					findRes->second += value;
				}
				else
				{
					map[vertex] = value;
				}
			}
			vToFContrib[f] = map;
		}

		EdgeData<Vector3> splitEdgePositions(mesh);
		for (Edge e : mesh.edges()) {
			std::array<Face, 2> neigh{ e.halfedge().face(), e.halfedge().twin().face() };
			splitEdgePositions[e] = (splitFacePositions[neigh[0]] + splitFacePositions[neigh[1]]) / 2.;

			std::unordered_map<Vertex, float> map{};
			for (auto const & [vertex, value] : vToFContrib[neigh[0]])
			{
				float value2 = value * 0.5f;
				auto findRes = map.find(vertex);
				if (findRes != map.end())
				{
					findRes->second += value2;
				}
				else
				{
					map[vertex] = value2;
				}
			}
			for (auto const& [vertex, value] : vToFContrib[neigh[1]])
			{
				float value2 = value * 0.5f;
				auto findRes = map.find(vertex);
				if (findRes != map.end())
				{
					findRes->second += value2;
				}
				else
				{
					map[vertex] = value2;
				}
			}
			vToEContrib[e] = map;
		}

		for (Vertex v : mesh.vertices()) {

			double D = (double)v.degree();

			Vector3 S = geo.inputVertexPositions[v];

			std::unordered_map<Vertex, float> map{};
			
			map[v] = (D - 3) / D;

			Vector3 R = Vector3::zero();

			for (Edge e : v.adjacentEdges()) {
				R += splitEdgePositions[e] / D;
				for (auto const & [vertex, value] : vToEContrib[e])
				{
					float value2 = value * (2.0f / (D * D));
					auto findRes = map.find(vertex);
					if (findRes != map.end())
					{
						findRes->second += value2;
					}
					else
					{
						map[vertex] = value2;
					}
				}
			}

			Vector3 Q = Vector3::zero();

			for (Face f : v.adjacentFaces()) {
				Q += splitFacePositions[f] / D;
				for (auto const& [vertex, value] : vToFContrib[f])
				{
					float value2 = value * (1.0f / (D * D));
					auto findRes = map.find(vertex);
					if (findRes != map.end())
					{
						findRes->second += value2;
					}
					else
					{
						map[vertex] = value2;
					}
				}
			}

			newPositions[v] = (Q + 2 * R + (D - 3) * S) / D;

			vToVContrib[v] = map;
		}

		// Subdivide edges
		VertexData<bool> isOrigVert(mesh, true);
		EdgeData<bool> isOrigEdge(mesh, true);
		for (Edge e : mesh.edges()) {
			if (!isOrigEdge[e]) continue;

			Vector3 newPos = splitEdgePositions[e];

			// split the edge
			Halfedge newHe = mesh.insertVertexAlongEdge(e);
			Vertex newV = newHe.vertex();

			isOrigVert[newV] = false;
			isOrigEdge[newHe.edge()] = false;
			isOrigEdge[newHe.twin().next().edge()] = false;

			GC_SAFETY_ASSERT(isOrigVert[newHe.twin().vertex()] && isOrigVert[newHe.twin().next().twin().vertex()], "???");

			newPositions[newV] = newPos;

			vToVContrib[newV] = {};
			for (auto& [v, value] : vToEContrib[e])
			{
				vToVContrib[newV][v] = value;
			}
		}

		// Subdivide faces
		FaceData<bool> isOrigFace(mesh, true);
		for (Face f : mesh.faces()) {
			if (!isOrigFace[f]) continue;

			Vector3 newPos = splitFacePositions[f];

			// split the face
			Vertex newV = mesh.insertVertex(f);
			isOrigVert[newV] = false;
			newPositions[newV] = newPos;

			vToVContrib[newV] = {};
			for (auto& [v, value] : vToFContrib[f])
			{
				vToVContrib[newV][v] = value;
			}
			
			for (Face f : newV.adjacentFaces()) {
				isOrigFace[f] = false;
			}

			// get incoming halfedges before we start deleting edges
			std::vector<Halfedge> incomingHalfedges;
			for (Halfedge he : newV.incomingHalfedges()) {
				incomingHalfedges.push_back(he);
			}

			for (Halfedge he : incomingHalfedges) {
				if (isOrigVert[he.vertex()]) {
					Face mergedF = mesh.removeEdge(he.edge());
					if (mergedF == Face()) {
						std::cout << "merge was impossible?" << std::endl;
					}
					else {
						isOrigFace[mergedF] = false;
					}
				}
			}
		}

		std::vector<ContribTuple> prevToNextContrib{};

		for (auto const & [nextV, items] : vToVContrib)
		{
			for (auto const & [prevV, value] : items)
			{
				prevToNextContrib.emplace_back(std::tuple{ prevV, nextV, value });
			}
		}

		/*for (auto & [f, items] : vToFContrib)
		{
			for (auto & [v, value] : items)
			{
				prevToNextContrib.emplace_back(std::tuple{ oldPositions[v], splitFacePositions[f], value });
			}
		}
		for (auto & [e, items] : vToEContrib)
		{
			for (auto& [v, value] : items)
			{
				prevToNextContrib.emplace_back(std::tuple{ oldPositions[v], splitEdgePositions[e], value});
			}
		}*/
		/*for (auto & [newV, items] : vToVContrib)
		{
			for (auto& [oldV, value] : items)
			{
				prevToNextContrib.emplace_back(std::tuple{ oldPositions[oldV], newPositions[newV], value});
			}
		}*/

		mesh.compress();
		geo.inputVertexPositions = newPositions;
		geo.refreshQuantities();

		auto const nextLvlVs = geo.vertexPositions.toVector();

		return std::make_unique<ContributionMap>(prevLvlVs, nextLvlVs, prevToNextContrib);
	}

	//--------------------------------------------------------------------------------------------------------

}
