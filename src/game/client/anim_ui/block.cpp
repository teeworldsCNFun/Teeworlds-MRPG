#include "block.h"

#include <game/client/render.h>

CBlock::CBlock(class CAnimUI* pAnimUI, const char *Name) : CAnimElementsUI(pAnimUI, Name)
{
	m_CornerType = CUI::CORNER_ALL;
	m_RoundCorner = 0.0f;
}

void CBlock::Render()
{
	CUIRect Rect = { m_pPos->GetValue().x, m_pPos->GetValue().y, m_pPos->GetValue().w, m_pPos->GetValue().h };
	RenderTools()->DrawUIRect(&Rect, m_pColor->GetValue(), (int)m_CornerType, m_RoundCorner);
}

void CBlock::Set(float RoundCorner, int CornerType)
{
	m_RoundCorner = RoundCorner;
	m_CornerType = CornerType;
}


