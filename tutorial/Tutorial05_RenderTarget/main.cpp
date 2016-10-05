/*
 * main.cpp (Tutorial05_RenderTarget)
 *
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "../tutorial.h"


// Enable multi-sampling
#define ENABLE_MULTISAMPLING

// Enable custom multi-sampling by rendering directly into a multi-sample texture
//#define ENABLE_CUSTOM_MULTISAMPLING


#ifndef ENABLE_MULTISAMPLING
#undef ENABLE_CUSTOM_MULTISAMPLING
#endif

class Tutorial05 : public Tutorial
{

    LLGL::ShaderProgram*    shaderProgram       = nullptr;

    LLGL::GraphicsPipeline* pipeline            = nullptr;

    LLGL::Buffer*           vertexBuffer        = nullptr;
    LLGL::Buffer*           indexBuffer         = nullptr;
    LLGL::Buffer*           constantBuffer      = nullptr;

    LLGL::Texture*          colorMap            = nullptr;
    LLGL::Sampler*          samplerState        = nullptr;

    LLGL::RenderTarget*     renderTarget        = nullptr;
    LLGL::Texture*          renderTargetTex     = nullptr;

    Gs::Matrix4f            renderTargetProj;

    const Gs::Vector2ui     renderTargetSize    = Gs::Vector2ui(
        #ifdef ENABLE_CUSTOM_MULTISAMPLING
        64
        #else
        512
        #endif
    );

    struct Settings
    {
        Gs::Matrix4f        wvpMatrix;
        int                 useTexture2DMS = 0;
        int                 _pad0[3];
    }
    settings;

public:

    Tutorial05() :
        Tutorial( L"LLGL Tutorial 05: RenderTarget" )
    {
        // Create all graphics objects
        auto vertexFormat = CreateBuffers();
        shaderProgram = LoadStandardShaderProgram(vertexFormat);
        CreatePipelines();
        CreateColorMap();
        CreateRenderTarget();

        // Show some information
        std::cout << "press LEFT MOUSE BUTTON and move the mouse on the X-axis to rotate the OUTER cube" << std::endl;
        std::cout << "press RIGHT MOUSE BUTTON and move the mouse on the X-axis to rotate the INNER cube" << std::endl;
        std::cout << "press RETURN KEY to save the render target texture to a PNG file" << std::endl;
    }

    LLGL::VertexFormat CreateBuffers()
    {
        // Specify vertex format
        LLGL::VertexFormat vertexFormat;
        vertexFormat.AppendAttribute({ "position", LLGL::DataType::Float, 3 });
        vertexFormat.AppendAttribute({ "texCoord", LLGL::DataType::Float, 2 });

        // Initialize vertices (scale texture-coordiantes a little bit, to show the texture border)
        auto vertices = GenerateTexturedCubeVertices();

        for (auto& v : vertices)
            v.texCoord = (v.texCoord - Gs::Vector2f(0.5f))*1.05f + Gs::Vector2f(0.5f);

        // Create vertex, index, and constant buffer
        vertexBuffer = CreateVertexBuffer(vertices, vertexFormat);
        indexBuffer = CreateIndexBuffer(GenerateTexturedCubeTriangleIndices(), LLGL::DataType::UInt32);
        constantBuffer = CreateConstantBuffer(settings);

        return vertexFormat;
    }

    void CreatePipelines()
    {
        // Create common graphics pipeline for scene rendering
        LLGL::GraphicsPipelineDescriptor pipelineDesc;
        {
            pipelineDesc.shaderProgram                  = shaderProgram;

            pipelineDesc.depth.testEnabled              = true;
            pipelineDesc.depth.writeEnabled             = true;

            pipelineDesc.rasterizer.cullMode            = LLGL::CullMode::Back;
            #ifdef ENABLE_MULTISAMPLING
            pipelineDesc.rasterizer.sampling.enabled    = true;
            #endif
        }
        pipeline = renderer->CreateGraphicsPipeline(pipelineDesc);
    }

    void CreateColorMap()
    {
        // Load color map texture from file
        colorMap = LoadTexture("colorMap.jpg");

        // Create common sampler state for all textures
        LLGL::SamplerDescriptor samplerDesc;
        {
            samplerDesc.textureWrapU    = LLGL::TextureWrap::Border;
            samplerDesc.textureWrapV    = LLGL::TextureWrap::Border;
            samplerDesc.maxAnisotropy   = 8;
            samplerDesc.borderColor     = { 0, 0, 0, 1 };
        }
        samplerState = renderer->CreateSampler(samplerDesc);
    }

    void CreateRenderTarget()
    {
        // Create render-target with multi-sampling
        #ifdef ENABLE_MULTISAMPLING
        unsigned int multiSamples = 8;
        #else
        unsigned int multiSamples = 0;
        #endif
        renderTarget = renderer->CreateRenderTarget(multiSamples);

        // Create empty render-target texture
        #ifdef ENABLE_CUSTOM_MULTISAMPLING
        
        renderTargetTex = renderer->CreateTexture(
            LLGL::Texture2DMSDesc(LLGL::TextureFormat::RGBA, renderTargetSize.x, renderTargetSize.y, multiSamples)
        );
        
        #else
        
        renderTargetTex = renderer->CreateTexture(
            LLGL::Texture2DDesc(LLGL::TextureFormat::RGBA, renderTargetSize.x, renderTargetSize.y)
        );

        #endif

        // Generate all MIP-map levels
        renderer->GenerateMips(*renderTargetTex);

        // Attach depth buffer to render-target
        renderTarget->AttachDepthBuffer(renderTargetSize);

        // Attach texture (first MIP-map level) to render-target
        renderTarget->AttachTexture(*renderTargetTex, {});

        // Initialize projection matrix for render-target scene rendering
        renderTargetProj = Gs::ProjectionMatrix4f::Perspective(1.0f, 0.1f, 100.0f, Gs::Deg2Rad(45.0f)).ToMatrix4();
    }

private:

    void UpdateModelTransform(const Gs::Matrix4f& proj, float rotation, const Gs::Vector3f& axis = { 0, 1, 0 })
    {
        settings.wvpMatrix = proj;
        Gs::Translate(settings.wvpMatrix, { 0, 0, 5 });
        Gs::RotateFree(settings.wvpMatrix, axis.Normalized(), rotation);
    }

    void OnDrawFrame() override
    {
        static const auto shaderStages = LLGL::ShaderStageFlags::VertexStage | LLGL::ShaderStageFlags::FragmentStage;

        // Update scene animation (simple rotation)
        static float rot0, rot1;

        if (input->KeyPressed(LLGL::Key::LButton))
            rot0 += static_cast<float>(input->GetMouseMotion().x)*0.005f;
        if (input->KeyPressed(LLGL::Key::RButton))
            rot1 += static_cast<float>(input->GetMouseMotion().x)*0.005f;

        // Set common buffers and sampler states
        context->SetIndexBuffer(*indexBuffer);
        context->SetVertexBuffer(*vertexBuffer);
        context->SetConstantBuffer(*constantBuffer, 0, shaderStages);
        context->SetSampler(*samplerState, 0, shaderStages);

        // Set graphics pipeline state
        context->SetGraphicsPipeline(*pipeline);

        if (IsOpenGL())
        {
            /*
            Set graphics API dependent state to be uniform between OpenGL and Direct3D:
            A huge difference between OpenGL and Direct3D is,
            that OpenGL stores image data from the lower-left to the upper-right in a texture,
            but Direct3D stores image data from the upper-left to the lower-right in a texture.
            The default screen-space origin of LLGL is the upper-left, so when rendering into a texture,
            we need to render vertically flipped when OpenGL is used.
            To do this we flip the Y-axis of the world-view-projection matrix and invert the front-facing,
            so that the face-culling works as excepted.
            */
            LLGL::GraphicsAPIDependentStateDescriptor apiState;
            {
                apiState.stateOpenGL.invertFrontFace = true;
            }
            context->SetGraphicsAPIDependentState(apiState);
        }

        // Draw scene into render-target
        context->SetRenderTarget(*renderTarget);
        {
            // Set viewport for render target
            context->SetViewport({ 0, 0, static_cast<float>(renderTargetSize.x), static_cast<float>(renderTargetSize.y) });

            // Clear color and depth buffers of active framebuffer (i.e. the render target)
            context->SetClearColor({ 0.2f, 0.7f, 0.1f });
            context->ClearBuffers(LLGL::ClearBuffersFlags::Color | LLGL::ClearBuffersFlags::Depth);

            // Set color map texture
            context->SetTexture(*colorMap, 0, shaderStages);

            // Update model transformation with render-target projection
            UpdateModelTransform(renderTargetProj, rot1, Gs::Vector3f(1));
            
            if (IsOpenGL())
            {
                /*
                Now flip the Y-axis (0 for X-axis, 1 for Y-axis, 2 for Z-axis) of the
                world-view-projection matrix to render vertically flipped into the render-target
                */
                Gs::FlipAxis(settings.wvpMatrix, 1);
            }

            #ifdef ENABLE_CUSTOM_MULTISAMPLING
            
            // Disable multi-sample texture in fragment shader
            settings.useTexture2DMS = 0;

            #endif

            UpdateBuffer(constantBuffer, settings);

            // Draw scene
            context->DrawIndexed(36, 0);
        }
        context->UnsetRenderTarget();

        // Generate MIP-maps again after texture has been written by the render-target
        renderer->GenerateMips(*renderTargetTex);

        if (IsOpenGL())
        {
            // Reset graphics API dependent state
            context->SetGraphicsAPIDependentState({});
        }

        // Reset viewport for the screen
        auto resolution = context->GetVideoMode().resolution.Cast<float>();
        context->SetViewport({ 0, 0, resolution.x, resolution.y });

        // Clear color and depth buffers of active framebuffer (i.e. the screen)
        context->SetClearColor(defaultClearColor);
        context->ClearBuffers(LLGL::ClearBuffersFlags::Color | LLGL::ClearBuffersFlags::Depth);

        #ifdef ENABLE_CUSTOM_MULTISAMPLING
        
        // Set multi-sample render-target texture
        context->SetTexture(*renderTargetTex, 1, shaderStages);
        
        #else
        
        // Set render-target texture
        context->SetTexture(*renderTargetTex, 0, shaderStages);

        #endif

        #ifdef ENABLE_CUSTOM_MULTISAMPLING

        // Enable multi-sample texture in fragment shader
        settings.useTexture2DMS = 1;

        #endif

        // Update model transformation with standard projection
        UpdateModelTransform(projection, rot0);
        UpdateBuffer(constantBuffer, settings);

        // Draw scene
        context->DrawIndexed(36, 0);

        // Present result on the screen
        context->Present();

        // Check if user wants to sage the render target texture to file
        if (input->KeyDown(LLGL::Key::Return))
            SaveTexture(*renderTargetTex, "RenderTargetTexture.png");
    }

};

LLGL_IMPLEMENT_TUTORIAL(Tutorial05);



