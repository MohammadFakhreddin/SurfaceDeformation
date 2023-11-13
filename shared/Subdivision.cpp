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

		std::unordered_map<Face, std::vector<std::tuple<Vertex, float>>> vToFContrib{};
		std::unordered_map<Edge, std::vector<std::tuple<Vertex, float>>> vToEContrib{};
		std::unordered_map<Vertex, std::vector<std::tuple<Vertex, float>>> vToVContrib{};

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

			vToFContrib[f] = {};
			for (Vertex vertex : f.adjacentVertices()) 
			{
				vToFContrib[f].emplace_back(std::tuple{ vertex, 1.0f / D });
			}
		}

		EdgeData<Vector3> splitEdgePositions(mesh);
		for (Edge e : mesh.edges()) {
			std::array<Face, 2> neigh{ e.halfedge().face(), e.halfedge().twin().face() };
			splitEdgePositions[e] = (splitFacePositions[neigh[0]] + splitFacePositions[neigh[1]]) / 2.;

			vToEContrib[e] = {};
			for (auto const & [vertex, value] : vToFContrib[neigh[0]])
			{
				vToEContrib[e].emplace_back(std::tuple{ vertex, value * 0.5f });
			}
			for (auto const& [vertex, value] : vToFContrib[neigh[1]])
			{
				vToEContrib[e].emplace_back(std::tuple{ vertex, value * 0.5f });
			}
		}

		for (Vertex v : mesh.vertices()) {

			double D = (double)v.degree();

			Vector3 S = geo.inputVertexPositions[v];

			vToVContrib[v] = {};

			vToVContrib[v].emplace_back(std::tuple{ v, (D - 3) / D});

			Vector3 R = Vector3::zero();

			for (Edge e : v.adjacentEdges()) {
				R += splitEdgePositions[e] / D;
				for (auto const & [vertex, value] : vToEContrib[e])
				{
					vToVContrib[v].emplace_back(std::tuple{ vertex, value * (2.0f / (D * D)) });
				}
			}

			Vector3 Q = Vector3::zero();

			for (Face f : v.adjacentFaces()) {
				Q += splitFacePositions[f] / D;
				for (auto const& [vertex, value] : vToFContrib[f])
				{
					vToVContrib[v].emplace_back(std::tuple{ vertex, value * (1.0f / (D * D))});
				}
			}

			newPositions[v] = (Q + 2 * R + (D - 3) * S) / D;

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

		for (auto & [f, items] : vToFContrib)
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
		}
		for (auto & [newV, items] : vToVContrib)
		{
			for (auto& [oldV, value] : items)
			{
				prevToNextContrib.emplace_back(std::tuple{ oldPositions[oldV], newPositions[newV], value});
			}
		}

		mesh.compress();
		geo.inputVertexPositions = newPositions;
		geo.refreshQuantities();

		auto const nextLvlVs = geo.vertexPositions.toVector();

		return std::make_unique<ContributionMap>(prevLvlVs, nextLvlVs, prevToNextContrib);
	}

	//--------------------------------------------------------------------------------------------------------

}
