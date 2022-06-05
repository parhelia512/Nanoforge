#pragma once
#include <Common/Typedefs.h>
#include <ext/WindowsWrapper.h>
#include "util/MathUtil.h"
#include <RfgTools++/types/Vec3.h>
#include <DirectXMath.h>

enum CameraDirection { up, down, left, right, forward, backward };

//3D perspective camera used by renderer and scenes. All angles are stored in radians internally but exposed publically in degrees.
class Camera
{
public:
    void Init(const DirectX::XMVECTOR& initialPos, f32 initialFovDegrees, const DirectX::XMFLOAT2& screenDimensions, f32 nearPlane, f32 farPlane);

    void DoFrame(f32 deltaTime);
    void HandleResize(const DirectX::XMFLOAT2& screenDimensions);
    //Todo: Make an InputManager class which provides callbacks to input handlers and tracks key state
    void HandleInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void UpdateProjectionMatrix();
    void UpdateViewMatrix();
    DirectX::XMMATRIX GetViewProjMatrix();

    void Translate(const DirectX::XMVECTOR& translation);
    void Translate(CameraDirection moveDirection, bool sprint = false);
    void LookAt(const DirectX::XMVECTOR& target);

    [[nodiscard]] DirectX::XMVECTOR Up() const;
    [[nodiscard]] DirectX::XMVECTOR Down() const;
    [[nodiscard]] DirectX::XMVECTOR Right() const;
    [[nodiscard]] DirectX::XMVECTOR Left() const;
    [[nodiscard]] DirectX::XMVECTOR Forward() const;
    [[nodiscard]] DirectX::XMVECTOR Backward() const;
    [[nodiscard]] DirectX::XMVECTOR Position() const;
    [[nodiscard]] Vec3 PositionVec3() const;

    void UpdateRotationFromMouse(f32 mouseXDelta, f32 mouseYDelta);

    DirectX::XMMATRIX camView;
    DirectX::XMMATRIX camProjection;

    DirectX::XMVECTOR camPosition;
    DirectX::XMVECTOR camTarget;
    DirectX::XMVECTOR camUp;

    DirectX::XMVECTOR DefaultForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    DirectX::XMVECTOR DefaultRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR camForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    DirectX::XMVECTOR camRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX camRotationMatrix;

    [[nodiscard]] f32 GetFovDegrees() const { return ToDegrees(fovRadians_); }
    [[nodiscard]] f32 GetFovRadians() const { return fovRadians_; }
    [[nodiscard]] f32 GetAspectRatio() const { return aspectRatio_; }
    [[nodiscard]] f32 GetNearPlane() const { return nearPlane_; }
    [[nodiscard]] f32 GetFarPlane() const { return farPlane_; }
    [[nodiscard]] f32 GetLookSensitivity() const { return lookSensitivity_; }

    [[nodiscard]] f32 GetPitchDegrees() const { return ToDegrees(pitchRadians_); }
    [[nodiscard]] f32 GetYawDegrees() const { return ToDegrees(yawRadians_); }

    void SetFovDegrees(f32 fovDegrees) { fovRadians_ = ToRadians(fovDegrees); UpdateProjectionMatrix(); }
    void SetNearPlane(f32 nearPlane) { nearPlane_ = nearPlane; UpdateProjectionMatrix(); }
    void SetFarPlane(f32 farPlane) { farPlane_ = farPlane; UpdateProjectionMatrix(); }
    void SetLookSensitivity(f32 lookSensitivity) { lookSensitivity_ = lookSensitivity; }

    void SetPosition(f32 x, f32 y, f32 z);

    f32 Speed = 5.0f;
    f32 SprintSpeed = 10.0f;
    f32 MinSpeed = 0.1f;
    f32 MaxSpeed = 100.0f;
    bool InputActive = true; //If false does not respond to input

private:
    f32 fovRadians_ = 0.0f;
    f32 aspectRatio_ = 1.0f;
    f32 nearPlane_ = 1.0f;
    f32 farPlane_ = 100.0f;
    DirectX::XMFLOAT2 screenDimensions_ = {};

    f32 yawRadians_ = 0.0f;
    f32 pitchRadians_ = 0.0f;
    f32 minPitch_ = ToRadians(-89.0f);
    f32 maxPitch_ = ToRadians(89.0f);
    f32 lookSensitivity_ = 10.0f;

    //Todo: Have InputManager provide easier way to track this
    f32 lastMouseXPos = 0;
    f32 lastMouseYPos = 0;
    f32 lastMouseXDelta = 0;
    f32 lastMouseYDelta = 0;
    bool rightMouseButtonDown = false;
};