#include "d3d11_depth_stencil.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11DepthStencilState::D3D11DepthStencilState(
          D3D11Device*              device,
    const D3D11_DEPTH_STENCIL_DESC& desc)
  : m_device(device), m_desc(desc), m_d3d10(this) {
    m_state.enableDepthTest   = desc.DepthEnable;
    m_state.enableDepthWrite  = desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
    m_state.enableStencilTest = desc.StencilEnable;
    m_state.depthCompareOp    = DecodeCompareOp(desc.DepthFunc);
    m_state.stencilOpFront    = DecodeStencilOpState(desc.FrontFace, desc);
    m_state.stencilOpBack     = DecodeStencilOpState(desc.BackFace,  desc);
  }
  
  
  D3D11DepthStencilState::~D3D11DepthStencilState() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DepthStencilState::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11DepthStencilState)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10DepthStencilState)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D11DepthStencilState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11DepthStencilState::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  void STDMETHODCALLTYPE D3D11DepthStencilState::GetDesc(D3D11_DEPTH_STENCIL_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  void D3D11DepthStencilState::BindToContext(const Rc<DxvkContext>& ctx) {
    ctx->setDepthStencilState(m_state);
  }
  
  
  D3D11_DEPTH_STENCIL_DESC D3D11DepthStencilState::DefaultDesc() {
    D3D11_DEPTH_STENCILOP_DESC stencilOp;
    stencilOp.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    stencilOp.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    stencilOp.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
    stencilOp.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    
    D3D11_DEPTH_STENCIL_DESC dstDesc;
    dstDesc.DepthEnable      = TRUE;
    dstDesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
    dstDesc.DepthFunc        = D3D11_COMPARISON_LESS;
    dstDesc.StencilEnable    = FALSE;
    dstDesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
    dstDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    dstDesc.FrontFace        = stencilOp;
    dstDesc.BackFace         = stencilOp;
    return dstDesc;
  }
  
  
  HRESULT D3D11DepthStencilState::NormalizeDesc(D3D11_DEPTH_STENCIL_DESC* pDesc) {
    const D3D11_DEPTH_STENCIL_DESC defaultDesc = DefaultDesc();
    
    if (pDesc->DepthEnable) {
      pDesc->DepthEnable = TRUE;
      
      if (!ValidateDepthFunc(pDesc->DepthFunc))
        return E_INVALIDARG;
    } else {
      pDesc->DepthFunc      = defaultDesc.DepthFunc;
      pDesc->DepthWriteMask = defaultDesc.DepthWriteMask;
    }
    
    if (!ValidateDepthWriteMask(pDesc->DepthWriteMask))
      return E_INVALIDARG;
    
    if (pDesc->StencilEnable) {
      pDesc->StencilEnable = TRUE;
      
      if (!ValidateStencilFunc(pDesc->FrontFace.StencilFunc)
       || !ValidateStencilOp(pDesc->FrontFace.StencilFailOp)
       || !ValidateStencilOp(pDesc->FrontFace.StencilDepthFailOp)
       || !ValidateStencilOp(pDesc->FrontFace.StencilPassOp))
        return E_INVALIDARG;
      
      if (!ValidateStencilFunc(pDesc->BackFace.StencilFunc)
       || !ValidateStencilOp(pDesc->BackFace.StencilFailOp)
       || !ValidateStencilOp(pDesc->BackFace.StencilDepthFailOp)
       || !ValidateStencilOp(pDesc->BackFace.StencilPassOp))
        return E_INVALIDARG;
    } else {
      pDesc->FrontFace        = defaultDesc.FrontFace;
      pDesc->BackFace         = defaultDesc.BackFace;
      pDesc->StencilReadMask  = defaultDesc.StencilReadMask;
      pDesc->StencilWriteMask = defaultDesc.StencilWriteMask;
    }
    
    return S_OK;
  }
  
  
  VkStencilOpState D3D11DepthStencilState::DecodeStencilOpState(
    const D3D11_DEPTH_STENCILOP_DESC& StencilDesc,
    const D3D11_DEPTH_STENCIL_DESC&   Desc) const {
    VkStencilOpState result;
    result.failOp      = VK_STENCIL_OP_KEEP;
    result.passOp      = VK_STENCIL_OP_KEEP;
    result.depthFailOp = VK_STENCIL_OP_KEEP;
    result.compareOp   = VK_COMPARE_OP_ALWAYS;
    result.compareMask = Desc.StencilReadMask;
    result.writeMask   = Desc.StencilWriteMask;
    result.reference   = 0;
    
    if (Desc.StencilEnable) {
      result.failOp      = DecodeStencilOp(StencilDesc.StencilFailOp);
      result.passOp      = DecodeStencilOp(StencilDesc.StencilPassOp);
      result.depthFailOp = DecodeStencilOp(StencilDesc.StencilDepthFailOp);
      result.compareOp   = DecodeCompareOp(StencilDesc.StencilFunc);
    }
    
    return result;
  }
  
  
  VkStencilOp D3D11DepthStencilState::DecodeStencilOp(D3D11_STENCIL_OP Op) const {
    switch (Op) {
      case D3D11_STENCIL_OP_KEEP:       return VK_STENCIL_OP_KEEP;
      case D3D11_STENCIL_OP_ZERO:       return VK_STENCIL_OP_ZERO;
      case D3D11_STENCIL_OP_REPLACE:    return VK_STENCIL_OP_REPLACE;
      case D3D11_STENCIL_OP_INCR_SAT:   return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      case D3D11_STENCIL_OP_DECR_SAT:   return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
      case D3D11_STENCIL_OP_INVERT:     return VK_STENCIL_OP_INVERT;
      case D3D11_STENCIL_OP_INCR:       return VK_STENCIL_OP_INCREMENT_AND_WRAP;
      case D3D11_STENCIL_OP_DECR:       return VK_STENCIL_OP_DECREMENT_AND_WRAP;
      default:                          return VK_STENCIL_OP_KEEP;
    }
  }
  
  
  bool D3D11DepthStencilState::ValidateDepthFunc(D3D11_COMPARISON_FUNC Comparison) {
    return Comparison >= D3D11_COMPARISON_NEVER
        && Comparison <= D3D11_COMPARISON_ALWAYS;
  }
  
  
  bool D3D11DepthStencilState::ValidateStencilFunc(D3D11_COMPARISON_FUNC Comparison) {
    return Comparison >= D3D11_COMPARISON_NEVER
        && Comparison <= D3D11_COMPARISON_ALWAYS;
  }
  
  
  bool D3D11DepthStencilState::ValidateStencilOp(D3D11_STENCIL_OP StencilOp) {
    return StencilOp >= D3D11_STENCIL_OP_KEEP
        && StencilOp <= D3D11_STENCIL_OP_DECR;
  }
  
  
  bool D3D11DepthStencilState::ValidateDepthWriteMask(D3D11_DEPTH_WRITE_MASK Mask) {
    return Mask == D3D11_DEPTH_WRITE_MASK_ZERO 
        || Mask == D3D11_DEPTH_WRITE_MASK_ALL;
  }
  
}
