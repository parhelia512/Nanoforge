using System;
using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Render.Resources;
using Silk.NET.Vulkan;

namespace Nanoforge.Render;

public class Scene
{
    public List<RenderObject> RenderObjects = new();
    public Camera? Camera;

    private RenderContext? _context;
    public Framebuffer[]? SwapChainFramebuffers;
    private Texture2D[]? _renderTextures;
    private VkBuffer? _renderTextureBuffer;
    private CommandBuffer _renderImageCopyCmdBuffer;
    private Fence _renderImageCopyFence;
    public uint ViewportWidth;
    public uint ViewportHeight;
    private Texture2D? _depthTexture;
    public CommandBuffer[]? CommandBuffers;
    public int LastFrame = -1;

    private const uint DefaultViewportWidth = 1280;
    private const uint DefaultViewportHeight = 720;

    public void Init(RenderContext context)
    {
        _context = context;
        ViewportWidth = DefaultViewportWidth;
        ViewportHeight = DefaultViewportHeight;
        Camera = new(position: new Vector3(-2.5f, 3.0f, -2.5f), fovDegrees: 60.0f, new Vector2(DefaultViewportWidth, DefaultViewportHeight), nearPlane: 1.0f, farPlane: 10000000.0f);
        InitRenderTextures();
    }

    public void Update(SceneFrameUpdateParams updateParams)
    {
        Camera!.Update(updateParams);
    }

    public void ViewportResize(Vector2 viewportSize)
    {
        ViewportWidth = (uint)viewportSize.X;
        ViewportHeight = (uint)viewportSize.Y;
        _context!.Vk.DeviceWaitIdle(_context.Device);
        CleanupRenderTextures();
        InitRenderTextures();
    }
    
    public RenderObject CreateRenderObject(string materialName, Vector3 position, Matrix4x4 orient, Mesh mesh, Texture2D texture)
    {
        RenderObject renderObject = new(position, orient, mesh, texture, materialName);
        RenderObjects.Add(renderObject);
        return renderObject;
    }

    private void InitRenderTextures()
    {
        CreateRenderTextures();
        CreateDepthResources();
        CreateFramebuffers();
        CreateCommandBuffers();
    }
    
    private void CreateRenderTextures()
    {
        _renderTextures = new Texture2D[Renderer.MaxFramesInFlight];
        for (int i = 0; i < Renderer.MaxFramesInFlight; i++)
        {
            Texture2D renderTexture = new Texture2D(_context!, ViewportWidth, ViewportHeight, 1, Renderer.RenderTextureImageFormat, ImageTiling.Optimal,
                ImageUsageFlags.TransferSrcBit | ImageUsageFlags.ColorAttachmentBit, MemoryPropertyFlags.DeviceLocalBit,
                ImageAspectFlags.ColorBit);
            renderTexture.TransitionLayoutImmediate(ImageLayout.General);
            renderTexture.CreateImageView();

            _renderTextures![i] = renderTexture;
        }

        _renderTextureBuffer = new VkBuffer(_context!, ViewportWidth * ViewportHeight * 4, BufferUsageFlags.TransferDstBit,
            MemoryPropertyFlags.HostCachedBit | MemoryPropertyFlags.HostCoherentBit | MemoryPropertyFlags.HostVisibleBit);

        _renderImageCopyCmdBuffer = _context!.AllocateCommandBuffer();
        _renderImageCopyFence = _context.CreateFence();
    }
    
    private unsafe void CreateCommandBuffers()
    {
        CommandBuffers = new CommandBuffer[SwapChainFramebuffers!.Length];

        CommandBufferAllocateInfo allocInfo = new()
        {
            SType = StructureType.CommandBufferAllocateInfo,
            CommandPool = _context!.CommandPool,
            Level = CommandBufferLevel.Primary,
            CommandBufferCount = (uint)CommandBuffers.Length,
        };

        fixed (CommandBuffer* commandBuffersPtr = CommandBuffers)
        {
            if (_context!.Vk.AllocateCommandBuffers(_context!.Device, in allocInfo, commandBuffersPtr) != Result.Success)
            {
                throw new Exception("Failed to allocate command buffers!");
            }
        }
    }

    private unsafe void CreateFramebuffers()
    {
        SwapChainFramebuffers = new Framebuffer[Renderer.MaxFramesInFlight];

        for (int i = 0; i < Renderer.MaxFramesInFlight; i++)
        {
            ImageView[] attachments = [_renderTextures![i].ImageViewHandle, _depthTexture!.ImageViewHandle];

            fixed (ImageView* attachmentsPtr = attachments)
            {
                FramebufferCreateInfo framebufferInfo = new()
                {
                    SType = StructureType.FramebufferCreateInfo,
                    RenderPass = _context!.PrimaryRenderPass,
                    AttachmentCount = (uint)attachments.Length,
                    PAttachments = attachmentsPtr,
                    Width = ViewportWidth,
                    Height = ViewportHeight,
                    Layers = 1,
                };

                if (_context!.Vk.CreateFramebuffer(_context.Device, in framebufferInfo, null, out SwapChainFramebuffers[i]) != Result.Success)
                {
                    throw new Exception("failed to create framebuffer!");
                }
            }
        }
    }

    private void CreateDepthResources()
    {
        Format depthFormat = Renderer.DepthTextureFormat;

        _depthTexture = new Texture2D(_context!, ViewportWidth, ViewportHeight, 1, depthFormat, ImageTiling.Optimal, ImageUsageFlags.DepthStencilAttachmentBit,
            MemoryPropertyFlags.DeviceLocalBit, ImageAspectFlags.DepthBit);
        _depthTexture.CreateImageView();
    }
    
    private unsafe void CleanupRenderTextures()
    {
        _depthTexture!.Destroy();

        foreach (var framebuffer in SwapChainFramebuffers!)
        {
            _context!.Vk.DestroyFramebuffer(_context!.Device, framebuffer, null);
        }

        fixed (CommandBuffer* commandBuffersPtr = CommandBuffers)
        {
            _context!.Vk.FreeCommandBuffers(_context!.Device, _context!.CommandPool, (uint)CommandBuffers!.Length, commandBuffersPtr);
        }

        foreach (Texture2D texture in _renderTextures!)
        {
            texture.Destroy();
        }
        
        _renderTextureBuffer!.Destroy();
        _context!.Vk.FreeCommandBuffers(_context.Device, _context.CommandPool, 1, _renderImageCopyCmdBuffer);
        _context.Vk.DestroyFence(_context.Device, _renderImageCopyFence, null);
    }
    
    public unsafe void GetRenderImage(IntPtr address)
    {
        if (LastFrame == -1)
            return;
        
        _context!.BeginCommandBuffer(_renderImageCopyCmdBuffer);
        
        _renderTextures![LastFrame].TransitionLayout(_renderImageCopyCmdBuffer, ImageLayout.TransferSrcOptimal);
        _renderTextures![LastFrame].CopyToBuffer(_renderImageCopyCmdBuffer, _renderTextureBuffer!.VkHandle);
        _renderTextures![LastFrame].TransitionLayout(_renderImageCopyCmdBuffer, ImageLayout.General);

        _context.EndCommandBuffer(_renderImageCopyCmdBuffer, _renderImageCopyFence);
        _context.Vk.WaitForFences(_context.Device, 1, _renderImageCopyFence, true, ulong.MaxValue);
        _context.Vk.ResetFences(_context.Device, 1, _renderImageCopyFence);
        
        void* mappedData = null;
        _renderTextureBuffer!.MapMemory(ref mappedData);
        
        ulong numBytes = ViewportWidth * ViewportHeight * 4;
        System.Buffer.MemoryCopy(mappedData, address.ToPointer(), numBytes, numBytes);
        
        _renderTextureBuffer!.UnmapMemory();
    }

    public void Destroy()
    {
        CleanupRenderTextures();
        foreach (RenderObject renderObject in RenderObjects)
        {
            renderObject.Destroy();
        }
    }
}

public struct SceneFrameUpdateParams(float deltaTime, float totalTime, bool leftMouseButtonDown, bool rightMouseButtonDown, Vector2 mousePosition, Vector2 mousePositionDelta, bool mouseOverViewport)
{
    public readonly float DeltaTime = deltaTime;
    public readonly float TotalTime = totalTime;
    public readonly MouseState Mouse = new MouseState(leftMouseButtonDown, rightMouseButtonDown, mousePosition, mousePositionDelta, mouseOverViewport);
}

public struct MouseState(bool leftMouseButtonDown, bool rightMouseButtonDown, Vector2 position, Vector2 positionDelta, bool mouseOverViewport)
{
    public readonly bool LeftMouseButtonDown = leftMouseButtonDown;
    public readonly bool RightMouseButtonDown = rightMouseButtonDown;
    public readonly Vector2 Position = position;
    public readonly Vector2 PositionDelta = positionDelta;
    public readonly bool MouseOverViewport = mouseOverViewport;
}