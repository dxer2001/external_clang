//===- CIndexHigh.cpp - Higher level API functions ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "IndexingContext.h"

#include "clang/AST/RecursiveASTVisitor.h"

using namespace clang;
using namespace cxindex;

namespace {

class BodyIndexer : public RecursiveASTVisitor<BodyIndexer> {
  IndexingContext &IndexCtx;
  const NamedDecl *Parent;
  const DeclContext *ParentDC;

  typedef RecursiveASTVisitor<BodyIndexer> base;
public:
  BodyIndexer(IndexingContext &indexCtx,
              const NamedDecl *Parent, const DeclContext *DC)
    : IndexCtx(indexCtx), Parent(Parent), ParentDC(DC) { }
  
  bool shouldWalkTypesOfTypeLocs() const { return false; }

  bool TraverseTypeLoc(TypeLoc TL) {
    IndexCtx.indexTypeLoc(TL, Parent, ParentDC);
    return true;
  }

  bool TraverseNestedNameSpecifierLoc(NestedNameSpecifierLoc NNS) {
    IndexCtx.indexNestedNameSpecifierLoc(NNS, Parent, ParentDC);
    return true;
  }

  bool VisitDeclRefExpr(DeclRefExpr *E) {
    IndexCtx.handleReference(E->getDecl(), E->getLocation(),
                             Parent, ParentDC, E);
    return true;
  }

  bool VisitMemberExpr(MemberExpr *E) {
    IndexCtx.handleReference(E->getMemberDecl(), E->getMemberLoc(),
                             Parent, ParentDC, E);
    return true;
  }

  bool VisitDesignatedInitExpr(DesignatedInitExpr *E) {
    for (DesignatedInitExpr::reverse_designators_iterator
           D = E->designators_rbegin(), DEnd = E->designators_rend();
           D != DEnd; ++D) {
      if (D->isFieldDesignator())
        IndexCtx.handleReference(D->getField(), D->getFieldLoc(),
                                 Parent, ParentDC, E);
    }
    return true;
  }

  bool VisitObjCIvarRefExpr(ObjCIvarRefExpr *E) {
    IndexCtx.handleReference(E->getDecl(), E->getLocation(),
                             Parent, ParentDC, E);
    return true;
  }

  bool VisitObjCMessageExpr(ObjCMessageExpr *E) {
    if (TypeSourceInfo *Cls = E->getClassReceiverTypeInfo())
      IndexCtx.indexTypeSourceInfo(Cls, Parent, ParentDC);

    if (ObjCMethodDecl *MD = E->getMethodDecl())
      IndexCtx.handleReference(MD, E->getSelectorStartLoc(),
                               Parent, ParentDC, E,
                               E->isImplicit() ? CXIdxEntityRef_Implicit
                                               : CXIdxEntityRef_Direct);
    return true;
  }

  bool VisitObjCPropertyRefExpr(ObjCPropertyRefExpr *E) {
    if (E->isExplicitProperty())
      IndexCtx.handleReference(E->getExplicitProperty(), E->getLocation(),
                               Parent, ParentDC, E);

    // No need to do a handleReference for the objc method, because there will
    // be a message expr as part of PseudoObjectExpr.
    return true;
  }

  bool VisitObjCBoxedExpr(ObjCBoxedExpr *E) {
    if (ObjCMethodDecl *MD = E->getBoxingMethod())
      IndexCtx.handleReference(MD, E->getLocStart(),
                               Parent, ParentDC, E, CXIdxEntityRef_Implicit);
    return true;
  }
  
  bool VisitObjCDictionaryLiteral(ObjCDictionaryLiteral *E) {
    if (ObjCMethodDecl *MD = E->getDictWithObjectsMethod())
      IndexCtx.handleReference(MD, E->getLocStart(),
                               Parent, ParentDC, E, CXIdxEntityRef_Implicit);
    return true;
  }

  bool VisitObjCArrayLiteral(ObjCArrayLiteral *E) {
    if (ObjCMethodDecl *MD = E->getArrayWithObjectsMethod())
      IndexCtx.handleReference(MD, E->getLocStart(),
                               Parent, ParentDC, E, CXIdxEntityRef_Implicit);
    return true;
  }

  bool VisitCXXConstructExpr(CXXConstructExpr *E) {
    IndexCtx.handleReference(E->getConstructor(), E->getLocation(),
                             Parent, ParentDC, E);
    return true;
  }

  bool TraverseCXXOperatorCallExpr(CXXOperatorCallExpr *E) {
    if (E->getOperatorLoc().isInvalid())
      return true; // implicit.
    return base::TraverseCXXOperatorCallExpr(E);
  }

  bool VisitDeclStmt(DeclStmt *S) {
    if (IndexCtx.shouldIndexFunctionLocalSymbols())
      IndexCtx.indexDeclGroupRef(S->getDeclGroup());
    return true;
  }

  bool TraverseLambdaCapture(LambdaExpr::Capture C) {
    if (C.capturesThis())
      return true;

    if (IndexCtx.shouldIndexFunctionLocalSymbols())
      IndexCtx.handleReference(C.getCapturedVar(), C.getLocation(),
                               Parent, ParentDC);
    return true;
  }

};

} // anonymous namespace

void IndexingContext::indexBody(const Stmt *S, const NamedDecl *Parent,
                                const DeclContext *DC) {
  if (!S)
    return;

  if (DC == 0)
    DC = Parent->getLexicalDeclContext();
  BodyIndexer(*this, Parent, DC).TraverseStmt(const_cast<Stmt*>(S));
}
