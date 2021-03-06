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
#ifndef INCLUDED_SW_SOURCE_UIBASE_INC_CPTION_HXX
#define INCLUDED_SW_SOURCE_UIBASE_INC_CPTION_HXX

#include <svx/stddlg.hxx>
#include <vcl/fixed.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/edit.hxx>
#include <vcl/button.hxx>
#include "actctrl.hxx"

#include <com/sun/star/container/XNameAccess.hpp>

#include "wrtsh.hxx"
#include "optload.hxx"

class SwFieldMgr;
class SwView;


class SwCaptionDialog : public SvxStandardDialog
{
    VclPtr<Edit>        m_pTextEdit;
    VclPtr<ComboBox>    m_pCategoryBox;
    OUString const      m_sNone;
    TextFilterAutoConvert m_aTextFilter;
    VclPtr<FixedText>   m_pFormatText;
    VclPtr<ListBox>     m_pFormatBox;
    //#i61007# order of captions
    VclPtr<FixedText>   m_pNumberingSeparatorFT;
    VclPtr<Edit>        m_pNumberingSeparatorED;
    VclPtr<FixedText>   m_pSepText;
    VclPtr<Edit>        m_pSepEdit;
    VclPtr<FixedText>   m_pPosText;
    VclPtr<ListBox>     m_pPosBox;
    VclPtr<OKButton>    m_pOKButton;
    VclPtr<PushButton>  m_pAutoCaptionButton;
    VclPtr<PushButton>  m_pOptionButton;

    VclPtr<SwCaptionPreview> m_pPreview;

    SwView       &rView; // search per active, avoid View
    std::unique_ptr<SwFieldMgr> pMgr;      // pointer to save the include

    OUString     sCharacterStyle;
    bool         bCopyAttributes;
    bool        bOrderNumberingFirst; //#i61007# order of captions

    css::uno::Reference< css::container::XNameAccess >    xNameAccess;

    DECL_LINK(SelectHdl, ComboBox&, void);
    DECL_LINK(SelectListBoxHdl, ListBox&, void);
    DECL_LINK(ModifyHdl, Edit&, void);
    DECL_LINK(OptionHdl, Button *, void);
    DECL_LINK(CaptionHdl, Button *, void);

    virtual void Apply() override;

    void    DrawSample();
    void    ApplyCaptionOrder(); //#i61007# order of captions

    static OUString our_aSepTextSave; // Save caption separator text
public:
     SwCaptionDialog( vcl::Window *pParent, SwView &rV );
    virtual ~SwCaptionDialog() override;
    virtual void dispose() override;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
