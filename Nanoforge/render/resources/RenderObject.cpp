#include "RenderObject.h"
#include "util/Profiler.h"

RenderObject::RenderObject(const Mesh& mesh, const Vec3& position, const Mat3& rotation) : ObjectMesh(mesh), Position(position), Rotation(rotation)
{

}

void RenderObject::Draw(ComPtr<ID3D11DeviceContext> d3d11Context, Buffer& perObjectBuffer, Camera& cam)
{
    if (!Visible)
        return;

    PROFILER_FUNCTION();

    //Shader constants for this object
    PerObjectConstants constants;

    //Calculate matrices
    DirectX::XMMATRIX rotation = DirectX::XMMatrixSet
    (
        Rotation.rvec.x, Rotation.rvec.y, Rotation.rvec.z, 0.0f,
        Rotation.uvec.x, Rotation.uvec.y, Rotation.uvec.z, 0.0f,
        Rotation.fvec.x, Rotation.fvec.y, Rotation.fvec.z, 0.0f,
        0.0f,            0.0f,            0.0f,            1.0f
    );
    DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(Position.x, Position.y, Position.z);
    DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z);

    //Then calculate MVP matrix with Model * View * Projection
    constants.MVP = rotation * translation * scale; //First calculate model matrix
    constants.MVP = DirectX::XMMatrixTranspose(constants.MVP * cam.camView * cam.camProjection);
    constants.Rotation = rotation;
    constants.WorldPosition.m128_f32[0] = Position.x;
    constants.WorldPosition.m128_f32[1] = Position.y;
    constants.WorldPosition.m128_f32[2] = Position.z;
    constants.WorldPosition.m128_f32[3] = 1.0f;

    //Set constants
    perObjectBuffer.SetData(d3d11Context, &constants);

    //Bind textures
    if (UseTextures)
    {
        for (u32 i = 0; i < Textures.size(); i++)
        {
            auto texture = Textures[i];
            if (texture.has_value())
                texture.value().Bind(d3d11Context, i);
        }
    }

    ObjectMesh.Draw(d3d11Context, SubmeshOverride);
}