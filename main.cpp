#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dxgidebug.h>
#include <dxcapi.h>



#pragma comment(lib, "d3d12.lib")
#pragma	comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")


struct Vector4
{
	float x, y, r, z;
};






LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	switch (msg) {
	case WM_DESTROY:

		PostQuitMessage(0);
		return 0;
	}



	return DefWindowProc(hwnd, msg, wparam, lparam);


}


std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}






void Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}



void Log(const std::wstring& message) {
	OutputDebugStringA(ConvertString(message).c_str());
}



IDxcBlob* CompilShader(
	//Compilerするshaderファイルへのパス
	const std::wstring& filePath,
	//Compilerに使用するPrifile
	const wchar_t* profile,
	//初期化で生成したものを3っつ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	//1. hlslファイルを読む

	//ここからシェーダーをコンパイルする旨をログに出す
	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

	//hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);

	//読まなかったら止める

	assert(SUCCEEDED(hr));

	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;



	//2. Compileする

	LPCWSTR arguments[] = {
		//コンパイル対象のhlslファイル名
		filePath.c_str(),
		//エントリーポイントの指定、基本的にmain以外にはしない
		L"-E", L"main",
		//shaderProfileの設定
		L"-T", profile,
		//デバッグ用の情報を埋め込む
		L"-Zi", L"-Qembed_debug",
		//最適化を外しておく
		L"-Od",
		//メモリアウトは行優先
		L"-Zpr",
	};

	//実際にshaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		//読み込んだファイル
		&shaderSourceBuffer,
		//コンパイルオプション
		arguments,
		//コンパイルオプションの数
		_countof(arguments),
		//includeが含まれた諸々
		includeHandler,
		//コンパイル結果
		IID_PPV_ARGS(&shaderResult)
	);

	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));




	//3. 警告・エラーが出てないか確認する


	//けいこく・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		//警告・エラーダメゼッタイ
		assert(false);
	}




	//4. Compile結果を受け取って返す

	//コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したログを出す
	Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	//もう使わないログを出す
	shaderSource->Release();
	shaderResult->Release();
	//実行用のバイナリを返却
	return shaderBlob;







	
	

	
}




ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes)
{

	//頂点リソース用のヒープ設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	//UploadHeapを使う
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	//頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	//バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//リソースのサイズ。今回はVector4を3頂点分
	vertexResourceDesc.Width = sizeInBytes;
	//バッファの場合はこれらは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	//バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//実際に頂点リソースを作る
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE, &vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;

}







int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	
	OutputDebugStringA("Hello,DirectX!\n");


	WNDCLASS wc{};

	wc.lpfnWndProc = WindowProc;

	wc.lpszClassName = L"CG2WindowClass";

	wc.hInstance = GetModuleHandle(nullptr);

	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc);



	const int32_t kClientWidth = 1280;

	const int32_t kClientHeight = 720;

	


	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);




	HWND hwnd = CreateWindow(
		wc.lpszClassName,
		L"CG2",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr);


	//デバッグレイヤー
#ifdef _DEBUG

	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {

		//デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();

		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);

	}


#endif


	ShowWindow(hwnd, SW_SHOW);



	IDXGIFactory7* dxgiFactory = nullptr;

	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));















	IDXGIAdapter4* useAdapter = nullptr;

	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {

		DXGI_ADAPTER_DESC3 adapterDesc{ };
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Log(std::format(L"Use Adapater:{}\n", adapterDesc.Description));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	ID3D12Device* device = nullptr;

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};

	const char* featureLevelStrings[] = { "12.0", "12.1", "12.0" };

	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) {

			Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}

	assert(device != nullptr);
	Log("Complete create D3D12Device!!!\n");



#ifdef _DEBUG

	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {

		//やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

		//警告時に止まる
		//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);


		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		//指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);

		//解放
		infoQueue->Release();




	


	}

#endif



	








	//画面の色


	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));

	assert(SUCCEEDED(hr));


	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));

	assert(SUCCEEDED(hr));


	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));

	assert(SUCCEEDED(hr));






	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;
	swapChainDesc.Height = kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));








	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NumDescriptors = 2;
	hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));

	assert(SUCCEEDED(hr));





	ID3D12Resource* swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));

	assert(SUCCEEDED(hr));

	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));

	assert(SUCCEEDED(hr));



	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);

	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);








	/*typedef struct D3D12_CPU_DESCRIPTOR_HANDLE {
		SIZE_T ptr;
	}D3D12_CPU_D3D12_CPU_DESCRIPTOR_HANDLE;


	rtvHandles[0] = rtvStartHandle;

	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize();*/



	//初期値０でFenceを作る
	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	//FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);





	//dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	
	//現時点でincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));





	
	

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	//RootParameters作成。複数設定できるので配列。今回は結果一つだけなので長さ１の配列
	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	//CBVを使う
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	//PixelShaderで使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	//レジ下番号0とバインド
	rootParameters[0].Descriptor.ShaderRegister = 0;
	//ルートパラメータ配列へのポインタ
	descriptionRootSignature.pParameters = rootParameters;
	//配列の長さ
	descriptionRootSignature.NumParameters = _countof(rootParameters);



	//シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	//バイナリをもとに生成
	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));




	







	////RootSignature作成
	//D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	//descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;






	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayouDesc{};
	inputLayouDesc.pInputElementDescs = inputElementDescs;
	inputLayouDesc.NumElements = _countof(inputElementDescs);






	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	






	//RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;





	//Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompilShader(L"Object3D.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);



	
	IDxcBlob* pixelShaderBlob = CompilShader(L"Object3D.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);






	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	//RootSignature
	graphicsPipelineStateDesc.pRootSignature = rootSignature;
	//Inputlayout
	graphicsPipelineStateDesc.InputLayout = inputLayouDesc;
	//VertexShader
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };
	//PixelShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };
	//BleandState
	graphicsPipelineStateDesc.BlendState = blendDesc;
	//RasterizerState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定（気にしなくてよい）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//実際に生成
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));








	//頂点リリース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	//UploadHeapを使う
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	//頂点リリースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	//バッファーリソース、テクスチャの場合はまた別の設定
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//リソースサイズ。今回はVector4を三頂点分
	vertexResourceDesc.Width = sizeof(Vector4) * 3;
	//バッファの場合はこれらは１にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	//バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	





	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(Vector4) * 3);

	
	
	//実際に頂点リソースを作る
	//ID3D12Resource* vertexResource = nullptr;
	/*hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));*/





	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	//リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点3っつのサイズ
	vertexBufferView.SizeInBytes = sizeof(Vector4) * 3;
	//１頂点当たりのサイズ
	vertexBufferView.StrideInBytes = sizeof(Vector4);





	//頂点リソースにデータを書き込む
	Vector4* vertexData = nullptr;
	//書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	//左下
	vertexData[0] = { -0.5f, -0.5, 0.0f, 1.0f };
	//上
	vertexData[1] = { 0.0f, 0.5f, 0.0f, 1.0f };
	//右下
	vertexData[2] = { 0.5f, -0.5, 0.0f, 1.0f };




	


	//マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	ID3D12Resource* materialResouce = CreateBufferResource(device, sizeof(Vector4));
	//マテリアルにデータを書き込む
	Vector4* materialData = nullptr;
	//書き込むためのアドレスを取得
	materialResouce->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	//今回は赤を書き込んでみる
	*materialData = Vector4(1.0f, 0.0f, 0.0f, 1.0f);





	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	//シザー短形
	D3D12_RECT scrissorRect{};
	//基本的にビューポートと同じに短形が構成されるようにする
	scrissorRect.left = 0;
	scrissorRect.right = kClientWidth;
	scrissorRect.top = 0;
	scrissorRect.bottom = kClientHeight;



	








	
	








	MSG msg{};

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {


		



			UINT BackBufferIndex = swapChain->GetCurrentBackBufferIndex();


			//TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			
			//今回のバリアはtransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

			//Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			//バリアを張る対象のリソース。　現在のバックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[BackBufferIndex];

			//遷移前（現在）のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;

			//遷移後のResouceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			//TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);




			commandList->OMSetRenderTargets(1, &rtvHandles[BackBufferIndex], false, nullptr);

			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
			commandList->ClearRenderTargetView(rtvHandles[BackBufferIndex], clearColor, 0, nullptr);





			//viewportを設定
			commandList->RSSetViewports(1, &viewport);
			//Scrissorを設定
			commandList->RSSetScissorRects(1, &scrissorRect);
			//RootSignatureを設定。PSOに設定しているけど別途設定が必要
			commandList->SetGraphicsRootSignature(rootSignature);
			//PSOを設定
			commandList->SetPipelineState(graphicsPipelineState);
			//VBVを設定
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけばよい
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			//マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResouce->GetGPUVirtualAddress());
			//描画！（DrawCall/ドローコール）。3頂点で一つのインスタンス。インスタンスについては今後
			commandList->DrawInstanced(3, 1, 0, 0);

			//画面に書く処理はすべて終わり、画面に映すので、状態を遷移
			//今回はRenderTargetkaraからPreasentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			//TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			hr = commandList->Close();
			assert(SUCCEEDED(hr));



			ID3D12CommandList* commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, commandLists);

			swapChain->Present(1, 0);

	






			



		





			//Fenceの値を更新
			fenceValue++;

			//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにsignalを送る
			commandQueue->Signal(fence, fenceValue);


		
			//Fenceの値が指定したsignal値にたどり着いているか確認する
			//GetCompletedValueの初期価値はFence作成時に渡した初期価値
			if (fence->GetCompletedValue() < fenceValue)
			{
				//指定したsignalにたどり着いてないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);

				//イベントを待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}




			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));

			hr = commandList->Reset(commandAllocator, nullptr);
			assert(SUCCEEDED(hr));






		


		}
	}


	CloseHandle(fenceEvent);
	fence->Release();
	rtvDescriptorHeap->Release();
	swapChainResources[0]->Release();
	swapChainResources[1]->Release();
	swapChain->Release();
	commandList->Release();
	commandAllocator->Release();
	commandQueue->Release();
	device->Release();
	useAdapter->Release();
	dxgiFactory->Release();


	
	vertexResource->Release();
	graphicsPipelineState->Release();
	signatureBlob->Release();
	if (errorBlob) {
		errorBlob->Release();
	}
	rootSignature->Release();
	pixelShaderBlob->Release();
	vertexShaderBlob->Release();


	materialResouce->Release();


#ifdef _DEBUG

	debugController->Release();

#endif
	CloseWindow(hwnd);






	//リソースリークチェック
	IDXGIDebug1* debug;

	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();



		


	}



	return 0;
}