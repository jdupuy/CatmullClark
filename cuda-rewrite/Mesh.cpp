#include "Mesh.h"
#include <cuda.h>

/*******************************************************************************
 * FaceCount -- Returns the number of faces
 *
 */
 __device__ int32_t ccm_FaceCount(const cc_Mesh *mesh)
{
    return mesh->faceCount;
}


/*******************************************************************************
 * EdgeCount -- Returns the number of edges
 *
 */
 __device__ int32_t ccm_EdgeCount(const cc_Mesh *mesh)
{
    return mesh->edgeCount;
}


/*******************************************************************************
 * CreaseCount -- Returns the number of creases
 *
 */
 __device__ int32_t ccm_CreaseCount(const cc_Mesh *mesh)
{
    return ccm_EdgeCount(mesh);
}


/*******************************************************************************
 * HalfedgeCount -- Returns the number of halfedges
 *
 */
 __device__ int32_t ccm_HalfedgeCount(const cc_Mesh *mesh)
{
    return mesh->halfedgeCount;
}


/*******************************************************************************
 * VertexCount -- Returns the number of vertices
 *
 */
 __device__ int32_t ccm_VertexCount(const cc_Mesh *mesh)
{
    return mesh->vertexCount;
}


/*******************************************************************************
 * UvCount -- Returns the number of uvs
 *
 */
 __device__ int32_t ccm_UvCount(const cc_Mesh *mesh)
{
    return mesh->uvCount;
}


/*******************************************************************************
 * FaceCountAtDepth -- Returns the number of faces at a given subdivision depth
 *
 * The number of faces follows the rule
 *          F^{d+1} = H^d
 * Therefore, the number of halfedges at a given subdivision depth d>= 0 is
 *          F^d = 4^{d - 1} H^0,
 * where H0 denotes the number of half-edges of the control cage.
 *
 */
 __device__ int32_t ccm_FaceCountAtDepth_Fast(const cc_Mesh *cage, int32_t depth)
{
    assert(depth > 0);
    const int32_t H0 = ccm_HalfedgeCount(cage);

    return (H0 << ((depth - 1) << 1));
}

 __device__ int32_t ccm_FaceCountAtDepth(const cc_Mesh *cage, int32_t depth)
{
    if (depth == 0) {
        return ccm_FaceCount(cage);
    } else {
        return ccm_FaceCountAtDepth_Fast(cage, depth);
    }
}


/*******************************************************************************
 * EdgeCountAtDepth -- Returns the number of edges at a given subdivision depth
 *
 * The number of edges follows the rule
 *          E^{d+1} = 2 E^d + H^d
 * Therefore, the number of edges at a given subdivision depth d>= 0 is
 *          E^d = 2^{d - 1} (2 E^0 + (2^d - 1) H^0),
 * where H0 and E0 respectively denote the number of half-edges and edges
 * of the control cage.
 *
 */
 __device__ int32_t ccm_EdgeCountAtDepth_Fast(const cc_Mesh *cage, int32_t depth)
{
    assert(depth > 0);
    const int32_t E0 = ccm_EdgeCount(cage);
    const int32_t H0 = ccm_HalfedgeCount(cage);
    const int32_t tmp = ~(0xFFFFFFFF << depth); // (2^d - 1)

    return ((E0 << 1) + (tmp * H0)) << (depth - 1);
}

 __device__ int32_t ccm_EdgeCountAtDepth(const cc_Mesh *cage, int32_t depth)
{
    if (depth == 0) {
        return ccm_EdgeCount(cage);
    } else {
        return ccm_EdgeCountAtDepth_Fast(cage, depth);
    }
}


/*******************************************************************************
 * HalfedgeCountAtDepth -- Returns the number of halfedges at a given subd depth
 *
 * The number of halfedges is multiplied by 4 at each subdivision step.
 * Therefore, the number of halfedges at a given subdivision depth d>= 0 is
 *          4^d H0,
 * where H0 denotes the number of half-edges of the control cage.
 *
 */
 __device__ int32_t ccm_HalfedgeCountAtDepth(const cc_Mesh *cage, int32_t depth)
{
    const int32_t H0 = ccm_HalfedgeCount(cage);

    return H0 << (depth << 1);
}


/*******************************************************************************
 * CreaseCountAtDepth -- Returns the number of creases at a given subd depth
 *
 * The number of creases is multiplied by 2 at each subdivision step.
 * Therefore, the number of halfedges at a given subdivision depth d>= 0 is
 *          2^d C0,
 * where C0 denotes the number of creases of the control cage.
 *
 */
 __device__ int32_t ccm_CreaseCountAtDepth(const cc_Mesh *cage, int32_t depth)
{
    const int32_t C0 = ccm_CreaseCount(cage);

    return C0 << depth;
}


/*******************************************************************************
 * VertexCountAtDepth -- Returns the number of vertex points at a given subd depth
 *
 * The number of vertices follows the rule
 *          V^{d+1} = V^d + E^d + F^d
 * For a quad mesh, the number of vertices at a given subdivision depth d>= 0 is
 *          V^d = V0 + (2^{d} - 1) E0 + (2^{d} - 1)^2 F0,
 * where:
 * - V0 denotes the number of vertices of the control cage
 * - E0 denotes the number of edges of the control cage
 * - F0 denotes the number of faces of the control cage
 * Note that since the input mesh may contain non-quad faces, we compute
 * the first subdivision step by hand and then apply the formula.
 *
 */
 __device__ int32_t ccm_VertexCountAtDepth_Fast(const cc_Mesh *cage, int32_t depth)
{
    assert(depth > 0);
    const int32_t V0 = ccm_VertexCount(cage);
    const int32_t F0 = ccm_FaceCount(cage);
    const int32_t E0 = ccm_EdgeCount(cage);
    const int32_t H0 = ccm_HalfedgeCount(cage);
    const int32_t F1 = H0;
    const int32_t E1 = 2 * E0 + H0;
    const int32_t V1 = V0 + E0 + F0;
    const int32_t tmp =  ~(0xFFFFFFFF << (depth - 1)); // 2^{d-1} - 1

    return V1 + tmp * (E1 + tmp * F1);
}

 __device__ int32_t ccm_VertexCountAtDepth(const cc_Mesh *cage, int32_t depth)
{
    if (depth == 0) {
        return ccm_VertexCount(cage);
    } else {
        return ccm_VertexCountAtDepth_Fast(cage, depth);
    }
}


/*******************************************************************************
 * Halfedge data accessors
 *
 */

 __device__ int32_t ccm_HalfedgeTwinID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm__Halfedge(mesh, halfedgeID)->twinID;
}

 __device__ int32_t ccm_HalfedgeNextID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm__Halfedge(mesh, halfedgeID)->nextID;
}

 __device__ int32_t ccm_HalfedgePrevID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm__Halfedge(mesh, halfedgeID)->prevID;
}

 __device__ int32_t ccm_HalfedgeVertexID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm__Halfedge(mesh, halfedgeID)->vertexID;
}

 __device__ int32_t ccm_HalfedgeUvID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm__Halfedge(mesh, halfedgeID)->uvID;
}

 __device__ int32_t ccm_HalfedgeEdgeID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm__Halfedge(mesh, halfedgeID)->edgeID;
}

 __device__ int32_t ccm_HalfedgeFaceID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm__Halfedge(mesh, halfedgeID)->faceID;
}

 __device__ float ccm_HalfedgeSharpness(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm_CreaseSharpness(mesh, ccm_HalfedgeEdgeID(mesh, halfedgeID));
}

 __device__ cc_VertexPoint ccm_HalfedgeVertexPoint(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm_VertexPoint(mesh, ccm_HalfedgeVertexID(mesh, halfedgeID));
}

__device__ cc_VertexUv ccm_HalfedgeVertexUv(const cc_Mesh *mesh, int32_t halfedgeID)
{
    return ccm_Uv(mesh, ccm_HalfedgeUvID(mesh, halfedgeID));
}



 __device__ int32_t ccm_CreaseNextID(const cc_Mesh *mesh, int32_t edgeID)
{
    return ccm__Crease(mesh, edgeID)->nextID;
}

__device__ int32_t ccm_CreasePrevID(const cc_Mesh *mesh, int32_t edgeID)
{
    return ccm__Crease(mesh, edgeID)->prevID;
}

 __device__ float ccm_CreaseSharpness(const cc_Mesh *mesh, int32_t edgeID)
{
    return ccm__Crease(mesh, edgeID)->sharpness;
}

 __device__ int32_t ccm_HalfedgeFaceID_Quad(int32_t halfedgeID)
{
    return halfedgeID >> 2;
}



 __device__ int32_t ccm_HalfedgeNextID_Quad(int32_t halfedgeID)
{
    return ccm__ScrollFaceHalfedgeID_Quad(halfedgeID, +1);
}

 __device__ int32_t ccm_HalfedgePrevID_Quad(int32_t halfedgeID)
{
    return ccm__ScrollFaceHalfedgeID_Quad(halfedgeID, -1);
}

/*******************************************************************************
 * Vertex data accessors
 *
 */
 __device__ cc_VertexPoint ccm_VertexPoint(const cc_Mesh *mesh, int32_t vertexID)
{
    return mesh->vertexPoints[vertexID];
}
 __device__ cc_VertexUv ccm_Uv(const cc_Mesh *mesh, int32_t uvID)
{
    return mesh->uvs[uvID];
}


/*******************************************************************************
 * VertexToHalfedgeID -- Returns a halfedge ID that carries a given vertex
 *
 */
 __device__ int32_t ccm_VertexToHalfedgeID(const cc_Mesh *mesh, int32_t vertexID)
{
    return mesh->vertexToHalfedgeIDs[vertexID];
}


/*******************************************************************************
 * EdgeToHalfedgeID -- Returns a halfedge associated with a given edge
 *
 */
 __device__ int32_t ccm_EdgeToHalfedgeID(const cc_Mesh *mesh, int32_t edgeID)
{
    return mesh->edgeToHalfedgeIDs[edgeID];
}


/*******************************************************************************
 * FaceToHalfedgeID -- Returns a halfedge associated with a given face
 *
 */
 __device__ int32_t ccm_FaceToHalfedgeID(const cc_Mesh *mesh, int32_t faceID)
{
    return mesh->faceToHalfedgeIDs[faceID];
}

 __device__ int32_t ccm_FaceToHalfedgeID_Quad(int32_t faceID)
{
    return faceID << 2;
}


/*******************************************************************************
 * Vertex Halfedge Iteration
 *
 */
 __device__ int32_t ccm_NextVertexHalfedgeID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    const int32_t twinID = ccm_HalfedgeTwinID(mesh, halfedgeID);

    return twinID >= 0 ? ccm_HalfedgeNextID(mesh, twinID) : -1;
}

 __device__ int32_t ccm_PrevVertexHalfedgeID(const cc_Mesh *mesh, int32_t halfedgeID)
{
    const int32_t prevID = ccm_HalfedgePrevID(mesh, halfedgeID);

    return ccm_HalfedgeTwinID(mesh, prevID);
}


/*******************************************************************************
 * Create -- Allocates memory for a mesh of given vertex and halfedge count
 *
 */
 cc_Mesh *
ccm_Create(
    int32_t vertexCount,
    int32_t uvCount,
    int32_t halfedgeCount,
    int32_t edgeCount,
    int32_t faceCount
) {
    const int32_t halfedgeByteCount = halfedgeCount * sizeof(cc_Halfedge);
    const int32_t vertexByteCount = vertexCount * sizeof(cc_VertexPoint);
    const int32_t uvByteCount = uvCount * sizeof(cc_VertexUv);
    const int32_t creaseByteCount = edgeCount * sizeof(cc_Crease);
    cc_Mesh *mesh = (cc_Mesh *)malloc(sizeof(*mesh));

    mesh->vertexCount = vertexCount;
    mesh->uvCount = uvCount;
    mesh->halfedgeCount = halfedgeCount;
    mesh->edgeCount = edgeCount;
    mesh->faceCount = faceCount;
    mesh->vertexToHalfedgeIDs = (int32_t *)malloc(sizeof(int32_t) * vertexCount);
    mesh->edgeToHalfedgeIDs = (int32_t *)malloc(sizeof(int32_t) * edgeCount);
    mesh->faceToHalfedgeIDs = (int32_t *)malloc(sizeof(int32_t) * faceCount);
    mesh->halfedges = (cc_Halfedge *)malloc(halfedgeByteCount);
    mesh->creases = (cc_Crease *)malloc(creaseByteCount);
    mesh->vertexPoints = (cc_VertexPoint *)malloc(vertexByteCount);
    mesh->uvs = (cc_VertexUv *)malloc(uvByteCount);

    return mesh;
}


/*******************************************************************************
 * Release -- Releases memory used for a given mesh
 *
 */
 void ccm_Release(cc_Mesh *mesh)
{
    free(mesh->vertexToHalfedgeIDs);
    free(mesh->faceToHalfedgeIDs);
    free(mesh->edgeToHalfedgeIDs);
    free(mesh->halfedges);
    free(mesh->creases);
    free(mesh->vertexPoints);
    free(mesh->uvs);
    free(mesh);
}


/*******************************************************************************
 * FaceCountAtDepth -- Returns the accumulated number of faces up to a given subdivision depth
 *
 */
 __device__ int32_t ccs_CumulativeFaceCountAtDepth(const cc_Mesh *cage, int32_t depth)
{
    return ccs_CumulativeHalfedgeCountAtDepth(cage, depth) >> 2;
}

 __device__ int32_t ccs_CumulativeFaceCount(const cc_Subd *subd)
{
    return ccs_CumulativeFaceCountAtDepth(subd->cage, ccs_MaxDepth(subd));
}


/*******************************************************************************
 * EdgeCountAtDepth -- Returns the accumulated number of edges up to a given subdivision depth
 *
 */
 __device__ int32_t ccs_CumulativeEdgeCountAtDepth(const cc_Mesh *cage, int32_t depth)
{
    assert(depth >= 0);
    const int32_t H0 = ccm_HalfedgeCount(cage);
    const int32_t E0 = ccm_EdgeCount(cage);
    const int32_t H1 = H0 << 2;
    const int32_t E1 = (E0 << 1) + H0;
    const int32_t D = depth;
    const int32_t A = ~(0xFFFFFFFF << D); //  2^{d} - 1

    return (A * (6 * E1 + A * H1 - H1)) / 6;
}

 __device__ int32_t ccs_CumulativeEdgeCount(const cc_Subd *subd)
{
    return ccs_CumulativeEdgeCountAtDepth(subd->cage, ccs_MaxDepth(subd));
}


/*******************************************************************************
 * HalfedgeCount -- Returns the total number of halfedges stored by the subd
 *
 * The number of halfedges is multiplied by 4 at each subdivision step.
 * It follows that the number of half-edges is given by the formula
 *    H = H0 x sum_{d=0}^{D} 4^d
 *      = H0 (4^{D+1} - 1) / 3
 * where D denotes the maximum subdivision depth and H0 the number of
 * halfedges in the control mesh.
 *
 */
__device__ int32_t ccs_CumulativeHalfedgeCountAtDepth(const cc_Mesh *cage, int32_t maxDepth)
{
    assert(maxDepth >= 0);
    const int32_t D = maxDepth;
    const int32_t H0 = ccm_HalfedgeCount(cage);
    const int32_t H1 = H0 << 2;
    const int32_t tmp = ~(0xFFFFFFFF << (D << 1)); // (4^D - 1)

    return (H1 * tmp) / 3;
}

 __device__ int32_t ccs_CumulativeHalfedgeCount(const cc_Subd *subd)
{
    return ccs_CumulativeHalfedgeCountAtDepth(subd->cage, ccs_MaxDepth(subd));
}


/*******************************************************************************
 * CreaseCount -- Returns the total number of creases stored by the subd
 *
 * The number of creases is multiplied by 2 at each subdivision step.
 * It follows that the number of half-edges is given by the formula
 *    C = C0 x sum_{d=0}^{D} 2^d
 *      = C0 (2^{D+1} - 1)
 * where D denotes the maximum subdivision depth and C0 the number of
 * creases in the control mesh.
 *
 */
 __device__ int32_t ccs_CumulativeCreaseCountAtDepth(const cc_Mesh *cage, int32_t maxDepth)
{
    assert(maxDepth >= 0);
    const int32_t D = maxDepth;
    const int32_t C0 = ccm_CreaseCount(cage);
    const int32_t C1 = C0 << 1;
    const int32_t tmp = ~(0xFFFFFFFF << D); // (2^D - 1)

    return (C1 * tmp);
}

 __device__ int32_t ccs_CumulativeCreaseCount(const cc_Subd *subd)
{
    return ccs_CumulativeCreaseCountAtDepth(subd->cage, ccs_MaxDepth(subd));
}


/*******************************************************************************
 * CumulativeVertexCount -- Returns the total number of vertices computed by the subd
 *
 * The number of vertices increases according to the following formula at
 * each subdivision step:
 *  Vd+1 = Fd + Ed + Vd
 *
 */
__device__ int32_t ccs_CumulativeVertexCountAtDepth(const cc_Mesh *cage, int32_t depth)
{
    assert(depth >= 0);
    const int32_t V0 = ccm_VertexCount(cage);
    const int32_t F0 = ccm_FaceCount(cage);
    const int32_t E0 = ccm_EdgeCount(cage);
    const int32_t H0 = ccm_HalfedgeCount(cage);
    const int32_t F1 = H0;
    const int32_t E1 = 2 * E0 + H0;
    const int32_t V1 = V0 + E0 + F0;
    const int32_t D = depth;
    const int32_t A =  ~(0xFFFFFFFF << (D     ));     //  2^{d} - 1
    const int32_t B =  ~(0xFFFFFFFF << (D << 1)) / 3; // (4^{d} - 1) / 3

    return A * (E1 - (F1 << 1)) + B * F1 + D * (F1 - E1 + V1);
}

 __device__ int32_t ccs_CumulativeVertexCount(const cc_Subd *subd)
{
    return ccs_CumulativeVertexCountAtDepth(subd->cage, ccs_MaxDepth(subd));
}


/*******************************************************************************
 * MaxDepth -- Retrieve the maximum subdivision depth of the subd
 *
 */
 __device__ int32_t ccs_MaxDepth(const cc_Subd *subd)
{
    return subd->maxDepth;
}


/*******************************************************************************
 * Create -- Create a subd
 *
 */
 __device__ cc_Subd *ccs_Create(const cc_Mesh *cage, int32_t maxDepth)
{
    const int32_t halfedgeCount = ccs_CumulativeHalfedgeCountAtDepth(cage, maxDepth);
    const int32_t creaseCount = ccs_CumulativeCreaseCountAtDepth(cage, maxDepth);
    const int32_t vertexCount = ccs_CumulativeVertexCountAtDepth(cage, maxDepth);
    const size_t halfedgeByteCount = halfedgeCount * sizeof(cc_Halfedge_SemiRegular);
    const size_t creaseByteCount = creaseCount * sizeof(cc_Crease);
    const size_t vertexPointByteCount = vertexCount * sizeof(cc_VertexPoint);
    cc_Subd *subd = (cc_Subd *)malloc(sizeof(*subd));

    subd->maxDepth = maxDepth;
    subd->halfedges = (cc_Halfedge_SemiRegular *)malloc(halfedgeByteCount);
    subd->creases = (cc_Crease *)malloc(creaseByteCount);
    subd->vertexPoints = (cc_VertexPoint *)malloc(vertexPointByteCount);
    subd->cage = cage;

    return subd;
}


/*******************************************************************************
 * Release -- Releases memory used for a given subd
 *
 */
 void ccs_Release(cc_Subd *subd)
{
    free(subd->halfedges);
    free(subd->creases);
    free(subd->vertexPoints);
    free(subd);
}


/*******************************************************************************
 * Crease data accessors
 *
 * These accessors are hidden from the user because not all edges within
 * the subd map to a crease. In particular: any edge create within a face
 * does not have an associated crease. This is because such edges will never
 * be sharp by construction.
 *
 */


 __device__ float ccs_CreaseSharpness_Fast(const cc_Subd *subd, int32_t edgeID, int32_t depth)
{
    return ccs__Crease(subd, edgeID, depth)->sharpness;
}

__device__ float ccs_CreaseSharpness(const cc_Subd *subd, int32_t edgeID, int32_t depth)
{
    const int32_t creaseCount = ccm_CreaseCountAtDepth(subd->cage, depth);

    if (edgeID < creaseCount) {
        return ccs_CreaseSharpness_Fast(subd, edgeID, depth);
    } else {
        return 0.0f;
    }
}

__device__ int32_t ccs_CreaseNextID_Fast(const cc_Subd *subd, int32_t edgeID, int32_t depth)
{
    return ccs__Crease(subd, edgeID, depth)->nextID;
}

__device__ int32_t ccs_CreaseNextID(const cc_Subd *subd, int32_t edgeID, int32_t depth)
{
    const int32_t creaseCount = ccm_CreaseCountAtDepth(subd->cage, depth);

    if (edgeID < creaseCount) {
        return ccs_CreaseNextID_Fast(subd, edgeID, depth);
    } else {
        return edgeID;
    }
}

__device__ int32_t ccs_CreasePrevID_Fast(const cc_Subd *subd, int32_t edgeID, int32_t depth)
{
    return ccs__Crease(subd, edgeID, depth)->prevID;
}

__device__ int32_t ccs_CreasePrevID(const cc_Subd *subd, int32_t edgeID, int32_t depth)
{
    const int32_t creaseCount = ccm_CreaseCountAtDepth(subd->cage, depth);

    if (edgeID < creaseCount) {
        return ccs_CreasePrevID_Fast(subd, edgeID, depth);
    } else {
        return edgeID;
    }
}


/*******************************************************************************
 * Halfedge data accessors
 *
 */

__device__ cc_VertexUv
ccs_HalfedgeVertexUv(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    return cc__DecodeUv(ccs__HalfedgeVertexUvID(subd, halfedgeID, depth));
}

__device__ int32_t
ccs_HalfedgeVertexID(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    return ccs__Halfedge(subd, halfedgeID, depth)->vertexID;
}

__device__ int32_t
ccs_HalfedgeTwinID(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    return ccs__Halfedge(subd, halfedgeID, depth)->twinID;
}

__device__ int32_t
ccs_HalfedgeNextID(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    (void)subd;
    (void)depth;

    return ccm_HalfedgeNextID_Quad(halfedgeID);
}

__device__ int32_t
ccs_HalfedgePrevID(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    (void)subd;
    (void)depth;

    return ccm_HalfedgePrevID_Quad(halfedgeID);
}

__device__ int32_t
ccs_HalfedgeFaceID(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    (void)subd;
    (void)depth;

    return ccm_HalfedgeFaceID_Quad(halfedgeID);
}

__device__ int32_t
ccs_HalfedgeEdgeID(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    return ccs__Halfedge(subd, halfedgeID, depth)->edgeID;
}

__device__ float
ccs_HalfedgeSharpness(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    const int32_t edgeID = ccs_HalfedgeEdgeID(subd, halfedgeID, depth);

    return ccs_CreaseSharpness(subd, edgeID, depth);
}

__device__ cc_VertexPoint
ccs_HalfedgeVertexPoint(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    const int32_t vertexID = ccs_HalfedgeVertexID(subd, halfedgeID, depth);

    return ccs_VertexPoint(subd, vertexID, depth);
}


/*******************************************************************************
 * Vertex data accessors
 *
 */
__device__ cc_VertexPoint
ccs_VertexPoint(const cc_Subd *subd, int32_t vertexID, int32_t depth)
{
    assert(depth <= ccs_MaxDepth(subd) && depth > 0);
    const int32_t stride = ccs_CumulativeVertexCountAtDepth(subd->cage, depth - 1);

    return subd->vertexPoints[stride + vertexID];
}


/*******************************************************************************
 * Vertex halfedge iteration
 *
 */
__device__ int32_t
ccs_PrevVertexHalfedgeID(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    const int32_t prevID = ccs_HalfedgePrevID(subd, halfedgeID, depth);

    return ccs_HalfedgeTwinID(subd, prevID, depth);
}

__device__ int32_t
ccs_NextVertexHalfedgeID(const cc_Subd *subd, int32_t halfedgeID, int32_t depth)
{
    const int32_t twinID = ccs_HalfedgeTwinID(subd, halfedgeID, depth);

    return ccs_HalfedgeNextID(subd, twinID, depth);
}


/*******************************************************************************
 * Face to Halfedge Mapping
 *
 */
__device__ int32_t
ccs_FaceToHalfedgeID(const cc_Subd *subd, int32_t faceID, int32_t depth)
{
    (void)subd;
    (void)depth;

    return ccm_FaceToHalfedgeID_Quad(faceID);
}


/*******************************************************************************
 * Edge to Halfedge Mapping
 *
 * This procedure returns one of the ID of one of the halfedge that constitutes
 * the edge. This routine has O(depth) complexity.
 *
 */

__device__ int32_t
ccs_EdgeToHalfedgeID(
    const cc_Subd *subd,
    int32_t edgeID,
    int32_t depth
) {
#if 0 // recursive version
    if (depth > 1) {
        int32_t edgeCount = ccm_EdgeCountAtDepth_Fast(subd->cage, depth - 1);

        if /* [2E, 2E + H) */ (edgeID >= 2 * edgeCount) {
            int32_t halfedgeID = edgeID - 2 * edgeCount;
            int32_t nextID = ccm_NextFaceHalfedgeID_Quad(halfedgeID);

            return cc__Max(4 * halfedgeID + 1, 4 * nextID + 2);

        } else if /* [E, 2E) */ (edgeID >= edgeCount) {
            int32_t halfedgeID = ccs_EdgeToHalfedgeID(subd,
                                                      edgeID >> 1,
                                                      depth - 1);
            int32_t nextID = ccm_NextFaceHalfedgeID_Quad(halfedgeID);

            return 4 * nextID + 3;

        } else /* [0, E) */ {
            int32_t halfedgeID = ccs_EdgeToHalfedgeID(subd, edgeID >> 1, depth - 1);

            return 4 * halfedgeID + 0;
        }
    } else {
        return ccs__EdgeToHalfedgeID_First(subd->cage, edgeID);
    }
#else // non-recursive version
    uint32_t heap = 1u;
    int32_t edgeHalfedgeID = 0;
    int32_t heapDepth = depth;

    // build heap
    for (; heapDepth > 1; --heapDepth) {
        const int32_t edgeCount = ccm_EdgeCountAtDepth_Fast(subd->cage,
                                                            heapDepth - 1);

        if /* [2E, 2E + H) */ (edgeID >= 2 * edgeCount) {
            const int32_t halfedgeID = edgeID - 2 * edgeCount;
            const int32_t nextID = ccm_HalfedgeNextID_Quad(halfedgeID);

            edgeHalfedgeID = cc__Max(4 * halfedgeID + 1, 4 * nextID + 2);
            break;
        } else {
            heap = (heap << 1) | (edgeID & 1);
            edgeID>>= 1;
        }
    }

    // initialize root cfg
    if (heapDepth == 1) {
        edgeHalfedgeID = ccs__EdgeToHalfedgeID_First(subd->cage, edgeID);
    }

    // read heap
    while (heap > 1u) {
        if ((heap & 1u) == 1u) {
            const int32_t nextID = ccm_HalfedgeNextID_Quad(edgeHalfedgeID);

            edgeHalfedgeID = 4 * nextID + 3;
        } else {
            edgeHalfedgeID = 4 * edgeHalfedgeID + 0;
        }

        heap>>= 1;
    }

    return edgeHalfedgeID;
#endif
}





