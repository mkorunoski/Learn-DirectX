#include <windows.h>
#include <iostream>
#include <d3d11.h>
#include <D3DX11.h>
#include <d3dx11effect.h>
#include <windowsx.h>
#include <xnamath.h>

#define WIDTH 800
#define HEIGHT 600
#define ReleaseCOM(x) { if(x){ x->Release(); x = 0; } }

HWND ghMainWnd = 0;

ID3D11Device* device;
ID3D11DeviceContext* deviceContext;
D3D_FEATURE_LEVEL featureLevel;
UINT numQualityLevels;
IDXGISwapChain* swapChain;
ID3D11RenderTargetView* renderTargetView;
ID3D11Texture2D* depthStencilBuffer;
ID3D11DepthStencilView* depthStencilView;

XMVECTOR gray = { 0.1f, 0.1f, 0.1f, 1.0f };

XMMATRIX modelMatrix;
XMMATRIX viewMatrix;
XMMATRIX projectionMatrix;
XMMATRIX modelViewProjectionMatrix;

ID3DX11Effect* fx;
ID3DX11EffectTechnique* tech;
ID3D11InputLayout* inputLayout;

ID3DX11EffectMatrixVariable* fxModelViewProjectionMatrix;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 color;
};

Vertex vertices[] =
{
	{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
	{ XMFLOAT3( 1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
	{ XMFLOAT3( 1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
	{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }
};
UINT stride = sizeof(Vertex);
UINT offset = 0;

UINT indices[] =
{
	0, 1, 2,
	0, 2, 3
};

ID3D11Buffer* vb;
ID3D11Buffer* ib;


void InitD3D()
{
	D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_DEBUG, 0, 0, D3D11_SDK_VERSION, &device, &featureLevel, &deviceContext);

	device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &numQualityLevels);

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.Width = WIDTH;
	swapChainDesc.BufferDesc.Height = HEIGHT;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.Flags = 0;
	swapChainDesc.OutputWindow = ghMainWnd;
	swapChainDesc.SampleDesc.Count = 4;
	swapChainDesc.SampleDesc.Quality = numQualityLevels - 1;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Windowed = true;

	IDXGIDevice* dxgiDevice = 0;
	device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
	IDXGIAdapter* dxgiAdapter = 0;
	dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
	IDXGIFactory* dxgiFactory = 0;
	dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);
	dxgiFactory->CreateSwapChain(device, &swapChainDesc, &swapChain);
	ReleaseCOM(dxgiDevice);
	ReleaseCOM(dxgiAdapter);
	ReleaseCOM(dxgiFactory);

	ID3D11Texture2D* backBuffer = 0;
	swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
	device->CreateRenderTargetView(backBuffer, 0, &renderTargetView);
	ReleaseCOM(backBuffer);

	D3D11_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = WIDTH;
	depthStencilDesc.Height = HEIGHT;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 4;
	depthStencilDesc.SampleDesc.Quality = numQualityLevels - 1;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	device->CreateTexture2D(&depthStencilDesc, 0, &depthStencilBuffer);
	device->CreateDepthStencilView(depthStencilBuffer, 0, &depthStencilView);

	deviceContext->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

	D3D11_VIEWPORT vp;
	vp.Width = WIDTH;
	vp.Height = HEIGHT;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	deviceContext->RSSetViewports(1, &vp);

	modelMatrix = XMMatrixIdentity();

	FXMVECTOR eyePosition = { 0.0f, 0.0f, -5.0f };
	FXMVECTOR focusPosition = { 0.0f, 0.0f, 0.0f };
	FXMVECTOR upDirection = { 0.0f, 1.0f, 0.0f };
	viewMatrix = XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);

	projectionMatrix = XMMatrixPerspectiveFovLH(50.0f, float(WIDTH) / HEIGHT, 1.0, 1000.0f);

	modelViewProjectionMatrix = projectionMatrix * viewMatrix * modelMatrix;

	ID3D10Blob* compiledShader = 0;
	ID3D10Blob* compilationMsgs = 0;
	HRESULT hr = D3DX11CompileFromFile(L"FX/shader.fx", 0, 0, 0, "fx_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION, 0, 0, &compiledShader, &compilationMsgs, 0);

	if (compilationMsgs != 0)
	{
		MessageBoxA(0, (char*)compilationMsgs->GetBufferPointer(), 0, 0);
		ReleaseCOM(compiledShader);
	}

	D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(), 0, device, &fx);

	fxModelViewProjectionMatrix = fx->GetVariableByName("modelViewProjectionMatrix")->AsMatrix();

	D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	
	tech = fx->GetTechniqueByName("Tech");
	D3DX11_PASS_DESC passDesc;
	tech->GetPassByIndex(0)->GetDesc(&passDesc);
	device->CreateInputLayout(vertexDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &inputLayout);

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(Vertex) * 4;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = vertices;

	device->CreateBuffer(&vbd, &vinitData, &vb);

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * 6;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = indices;

	device->CreateBuffer(&ibd, &iinitData, &ib);
}

void Draw()
{
	deviceContext->ClearRenderTargetView(renderTargetView, reinterpret_cast<float*>(&gray));
	deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->IASetInputLayout(inputLayout);
		
	deviceContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	deviceContext->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);

	fxModelViewProjectionMatrix->SetMatrix((float*)&fxModelViewProjectionMatrix);

	D3DX11_TECHNIQUE_DESC techDesc;
	tech->GetDesc(&techDesc);
	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		tech->GetPassByIndex(p)->Apply(0, deviceContext);
		deviceContext->DrawIndexed(6, 0, 0);
	}

	swapChain->Present(0, 0);
}

bool InitWindowsApp(HINSTANCE instanceHandle, int show);
int Run();
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd)
{
	if (!InitWindowsApp(hInstance, nShowCmd))
		return 0;

	InitD3D();

	return Run();
}

bool InitWindowsApp(HINSTANCE instanceHandle, int show)
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instanceHandle;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"BasicWndClass";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass FAILED", 0, 0);
		return false;
	}

	ghMainWnd = CreateWindow(
		L"BasicWndClass",
		L"DirectX",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		0,
		0,
		instanceHandle,
		0);

	if (ghMainWnd == 0)
	{
		MessageBox(0, L"CreateWindow FAILED", 0, 0);
		return false;
	}

	ShowWindow(ghMainWnd, show);
	UpdateWindow(ghMainWnd);

	return true;
}
int Run()
{
	MSG msg = { 0 };
	BOOL bRet = 1;

	while ((bRet = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			MessageBox(0, L"GetMessage FAILED", L"Error", MB_OK);
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Draw();
	}

	return (int)msg.wParam;
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		MessageBox(0, L"Hello, World", L"Hello", MB_OK);
		return 0;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			DestroyWindow(ghMainWnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}