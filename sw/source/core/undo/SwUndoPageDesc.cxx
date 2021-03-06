/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <doc.hxx>
#include <IDocumentUndoRedo.hxx>
#include <swundo.hxx>
#include <pagedesc.hxx>
#include <SwUndoPageDesc.hxx>
#include <SwRewriter.hxx>
#include <undobj.hxx>
#include <strings.hrc>
#include <fmtcntnt.hxx>
#include <fmthdft.hxx>

SwUndoPageDesc::SwUndoPageDesc(const SwPageDesc & _aOld,
                               const SwPageDesc & _aNew,
                               SwDoc * _pDoc)
    : SwUndo( _aOld.GetName() != _aNew.GetName() ?
              SwUndoId::RENAME_PAGEDESC :
              SwUndoId::CHANGE_PAGEDESC,
              _pDoc ),
      aOld(_aOld, _pDoc), aNew(_aNew, _pDoc), pDoc(_pDoc), bExchange( false )
{
    OSL_ENSURE(nullptr != pDoc, "no document?");

    /*
    The page description changes.
    If there are no header/footer content changes like header on/off or change from shared content
    to unshared etc., there is no reason to duplicate the content nodes (Crash i55547)
    But this happens, this Undo Ctor will destroy the unnecessary duplicate and manipulate the
    content pointer of the both page descriptions.
    */
    SwPageDesc &rOldDesc = aOld.m_PageDesc;
    SwPageDesc &rNewDesc = aNew.m_PageDesc;
    const SwFormatHeader& rOldHead = rOldDesc.GetMaster().GetHeader();
    const SwFormatHeader& rNewHead = rNewDesc.GetMaster().GetHeader();
    const SwFormatFooter& rOldFoot = rOldDesc.GetMaster().GetFooter();
    const SwFormatFooter& rNewFoot = rNewDesc.GetMaster().GetFooter();
    /* bExchange must not be set, if the old page descriptor will stay active.
    Two known situations:
    #i67735#: renaming a page descriptor
    #i67334#: changing the follow style
    If header/footer will be activated or deactivated, this undo will not work.
    */
    bExchange = ( aOld.GetName() == aNew.GetName() ) &&
        ( _aOld.GetFollow() == _aNew.GetFollow() ) &&
        ( rOldHead.IsActive() == rNewHead.IsActive() ) &&
        ( rOldFoot.IsActive() == rNewFoot.IsActive() );
    if( rOldHead.IsActive() && ( rOldDesc.IsHeaderShared() != rNewDesc.IsHeaderShared() ) )
        bExchange = false;
    if( rOldFoot.IsActive() && ( rOldDesc.IsFooterShared() != rNewDesc.IsFooterShared() ) )
        bExchange = false;
    if( ( rOldHead.IsActive() || rOldFoot.IsActive() ) && ( rOldDesc.IsFirstShared() != rNewDesc.IsFirstShared() ) )
        bExchange = false;
    if( bExchange )
    {
        if( rNewHead.IsActive() )
        {
            SwFrameFormat* pFormat = new SwFrameFormat( *rNewHead.GetHeaderFormat() );
            // The Ctor of this object will remove the duplicate!
            SwFormatHeader aFormatHeader(pFormat);
            (void)aFormatHeader;
            if (!rNewDesc.IsHeaderShared())
            {
                pFormat = new SwFrameFormat( *rNewDesc.GetLeft().GetHeader().GetHeaderFormat() );
                // The Ctor of this object will remove the duplicate!
                SwFormatHeader aLeftHeader(pFormat);
                (void)aLeftHeader;
            }
            if (!rNewDesc.IsFirstShared())
            {
                pFormat = new SwFrameFormat( *rNewDesc.GetFirstMaster().GetHeader().GetHeaderFormat() );
                // The Ctor of this object will remove the duplicate!
                SwFormatHeader aFirstHeader(pFormat);
                (void)aFirstHeader;
            }
        }
        // Same procedure for footers...
        if( rNewFoot.IsActive() )
        {
            SwFrameFormat* pFormat = new SwFrameFormat( *rNewFoot.GetFooterFormat() );
            // The Ctor of this object will remove the duplicate!
            SwFormatFooter aFormatFooter(pFormat);
            (void)aFormatFooter;
            if (!rNewDesc.IsFooterShared())
            {
                pFormat = new SwFrameFormat( *rNewDesc.GetLeft().GetFooter().GetFooterFormat() );
                // The Ctor of this object will remove the duplicate!
                SwFormatFooter aLeftFooter(pFormat);
                (void)aLeftFooter;
            }
            if (!rNewDesc.IsFirstShared())
            {
                pFormat = new SwFrameFormat( *rNewDesc.GetFirstMaster().GetFooter().GetFooterFormat() );
                // The Ctor of this object will remove the duplicate!
                SwFormatFooter aFirstFooter(pFormat);
                (void)aFirstFooter;
            }
        }

        // After this exchange method the old page description will point to zero,
        // the new one will point to the node position of the original content nodes.
        ExchangeContentNodes( aOld.m_PageDesc, aNew.m_PageDesc );
    }
}

SwUndoPageDesc::~SwUndoPageDesc()
{
}

void SwUndoPageDesc::ExchangeContentNodes( SwPageDesc& rSource, SwPageDesc &rDest )
{
    OSL_ENSURE( bExchange, "You shouldn't do that." );
    const SwFormatHeader& rDestHead = rDest.GetMaster().GetHeader();
    const SwFormatHeader& rSourceHead = rSource.GetMaster().GetHeader();
    if( rDestHead.IsActive() )
    {
        // Let the destination page description point to the source node position,
        // from now on this descriptor is responsible for the content nodes!
        const SfxPoolItem* pItem;
        rDest.GetMaster().GetAttrSet().GetItemState( RES_HEADER, false, &pItem );
        std::unique_ptr<SfxPoolItem> pNewItem(pItem->Clone());
        SwFrameFormat* pNewFormat = static_cast<SwFormatHeader*>(pNewItem.get())->GetHeaderFormat();
        pNewFormat->SetFormatAttr( rSourceHead.GetHeaderFormat()->GetContent() );

        // Let the source page description point to zero node position,
        // it loses the responsible and can be destroyed without removing the content nodes.
        rSource.GetMaster().GetAttrSet().GetItemState( RES_HEADER, false, &pItem );
        pNewItem.reset(pItem->Clone());
        pNewFormat = static_cast<SwFormatHeader*>(pNewItem.get())->GetHeaderFormat();
        pNewFormat->SetFormatAttr( SwFormatContent() );

        if( !rDest.IsHeaderShared() )
        {
            // Same procedure for unshared header..
            const SwFormatHeader& rSourceLeftHead = rSource.GetLeft().GetHeader();
            rDest.GetLeft().GetAttrSet().GetItemState( RES_HEADER, false, &pItem );
            pNewItem.reset(pItem->Clone());
            pNewFormat = static_cast<SwFormatHeader*>(pNewItem.get())->GetHeaderFormat();
            pNewFormat->SetFormatAttr( rSourceLeftHead.GetHeaderFormat()->GetContent() );
            rSource.GetLeft().GetAttrSet().GetItemState( RES_HEADER, false, &pItem );
            pNewItem.reset(pItem->Clone());
            pNewFormat = static_cast<SwFormatHeader*>(pNewItem.get())->GetHeaderFormat();
            pNewFormat->SetFormatAttr( SwFormatContent() );
        }
        if (!rDest.IsFirstShared())
        {
            // Same procedure for unshared header..
            const SwFormatHeader& rSourceFirstMasterHead = rSource.GetFirstMaster().GetHeader();
            rDest.GetFirstMaster().GetAttrSet().GetItemState( RES_HEADER, false, &pItem );
            pNewItem.reset(pItem->Clone());
            pNewFormat = static_cast<SwFormatHeader*>(pNewItem.get())->GetHeaderFormat();
            pNewFormat->SetFormatAttr( rSourceFirstMasterHead.GetHeaderFormat()->GetContent() );
            rSource.GetFirstMaster().GetAttrSet().GetItemState( RES_HEADER, false, &pItem );
            pNewItem.reset(pItem->Clone());
            pNewFormat = static_cast<SwFormatHeader*>(pNewItem.get())->GetHeaderFormat();
            pNewFormat->SetFormatAttr( SwFormatContent() );
        }
    }
    // Same procedure for footers...
    const SwFormatFooter& rDestFoot = rDest.GetMaster().GetFooter();
    const SwFormatFooter& rSourceFoot = rSource.GetMaster().GetFooter();
    if( !rDestFoot.IsActive() )
        return;

    const SfxPoolItem* pItem;
    rDest.GetMaster().GetAttrSet().GetItemState( RES_FOOTER, false, &pItem );
    std::unique_ptr<SfxPoolItem> pNewItem(pItem->Clone());
    SwFrameFormat *pNewFormat = static_cast<SwFormatFooter*>(pNewItem.get())->GetFooterFormat();
    pNewFormat->SetFormatAttr( rSourceFoot.GetFooterFormat()->GetContent() );

    rSource.GetMaster().GetAttrSet().GetItemState( RES_FOOTER, false, &pItem );
    pNewItem.reset(pItem->Clone());
    pNewFormat = static_cast<SwFormatFooter*>(pNewItem.get())->GetFooterFormat();
    pNewFormat->SetFormatAttr( SwFormatContent() );

    if( !rDest.IsFooterShared() )
    {
        const SwFormatFooter& rSourceLeftFoot = rSource.GetLeft().GetFooter();
        rDest.GetLeft().GetAttrSet().GetItemState( RES_FOOTER, false, &pItem );
        pNewItem.reset(pItem->Clone());
        pNewFormat = static_cast<SwFormatFooter*>(pNewItem.get())->GetFooterFormat();
        pNewFormat->SetFormatAttr( rSourceLeftFoot.GetFooterFormat()->GetContent() );
        rSource.GetLeft().GetAttrSet().GetItemState( RES_FOOTER, false, &pItem );
        pNewItem.reset(pItem->Clone());
        pNewFormat = static_cast<SwFormatFooter*>(pNewItem.get())->GetFooterFormat();
        pNewFormat->SetFormatAttr( SwFormatContent() );
    }
    if (rDest.IsFirstShared())
        return;

    const SwFormatFooter& rSourceFirstMasterFoot = rSource.GetFirstMaster().GetFooter();
    rDest.GetFirstMaster().GetAttrSet().GetItemState( RES_FOOTER, false, &pItem );
    pNewItem.reset(pItem->Clone());
    pNewFormat = static_cast<SwFormatFooter*>(pNewItem.get())->GetFooterFormat();
    pNewFormat->SetFormatAttr( rSourceFirstMasterFoot.GetFooterFormat()->GetContent() );
    rSource.GetFirstMaster().GetAttrSet().GetItemState( RES_FOOTER, false, &pItem );
    pNewItem.reset(pItem->Clone());
    pNewFormat = static_cast<SwFormatFooter*>(pNewItem.get())->GetFooterFormat();
    pNewFormat->SetFormatAttr( SwFormatContent() );
}

void SwUndoPageDesc::UndoImpl(::sw::UndoRedoContext &)
{
    // Move (header/footer)content node responsibility from new page descriptor to old one again.
    if( bExchange )
        ExchangeContentNodes( aNew.m_PageDesc, aOld.m_PageDesc );
    pDoc->ChgPageDesc(aOld.GetName(), aOld);
}

void SwUndoPageDesc::RedoImpl(::sw::UndoRedoContext &)
{
    // Move (header/footer)content node responsibility from old page descriptor to new one again.
    if( bExchange )
        ExchangeContentNodes( aOld.m_PageDesc, aNew.m_PageDesc );
    pDoc->ChgPageDesc(aNew.GetName(), aNew);
}

SwRewriter SwUndoPageDesc::GetRewriter() const
{
    SwRewriter aResult;

    aResult.AddRule(UndoArg1, aOld.GetName());
    aResult.AddRule(UndoArg2, SwResId(STR_YIELDS));
    aResult.AddRule(UndoArg3, aNew.GetName());

    return aResult;
}

SwUndoPageDescCreate::SwUndoPageDescCreate(const SwPageDesc * pNew,
                                           SwDoc * _pDoc)
    : SwUndo(SwUndoId::CREATE_PAGEDESC, _pDoc), pDesc(pNew), aNew(*pNew, _pDoc),
      pDoc(_pDoc)
{
    OSL_ENSURE(nullptr != pDoc, "no document?");
}

SwUndoPageDescCreate::~SwUndoPageDescCreate()
{
}

void SwUndoPageDescCreate::UndoImpl(::sw::UndoRedoContext &)
{
    if (pDesc)
    {
        aNew = *pDesc;
        pDesc = nullptr;
    }

    pDoc->DelPageDesc(aNew.GetName(), true);
}

void SwUndoPageDescCreate::DoImpl()
{
    SwPageDesc aPageDesc = aNew;
    pDoc->MakePageDesc(aNew.GetName(), &aPageDesc, false, true);
}

void SwUndoPageDescCreate::RedoImpl(::sw::UndoRedoContext &)
{
    DoImpl();
}

void SwUndoPageDescCreate::RepeatImpl(::sw::RepeatContext &)
{
    ::sw::UndoGuard const undoGuard(pDoc->GetIDocumentUndoRedo());
    DoImpl();
}

SwRewriter SwUndoPageDescCreate::GetRewriter() const
{
    SwRewriter aResult;

    if (pDesc)
        aResult.AddRule(UndoArg1, pDesc->GetName());
    else
        aResult.AddRule(UndoArg1, aNew.GetName());

    return aResult;
}

SwUndoPageDescDelete::SwUndoPageDescDelete(const SwPageDesc & _aOld,
                                           SwDoc * _pDoc)
    : SwUndo(SwUndoId::DELETE_PAGEDESC, _pDoc), aOld(_aOld, _pDoc), pDoc(_pDoc)
{
    OSL_ENSURE(nullptr != pDoc, "no document?");
}

SwUndoPageDescDelete::~SwUndoPageDescDelete()
{
}

void SwUndoPageDescDelete::UndoImpl(::sw::UndoRedoContext &)
{
    SwPageDesc aPageDesc = aOld;
    pDoc->MakePageDesc(aOld.GetName(), &aPageDesc, false, true);
}

void SwUndoPageDescDelete::DoImpl()
{
    pDoc->DelPageDesc(aOld.GetName(), true);
}

void SwUndoPageDescDelete::RedoImpl(::sw::UndoRedoContext &)
{
    DoImpl();
}

void SwUndoPageDescDelete::RepeatImpl(::sw::RepeatContext &)
{
    ::sw::UndoGuard const undoGuard(pDoc->GetIDocumentUndoRedo());
    DoImpl();
}

SwRewriter SwUndoPageDescDelete::GetRewriter() const
{
    SwRewriter aResult;

    aResult.AddRule(UndoArg1, aOld.GetName());

    return aResult;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
