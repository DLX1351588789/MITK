/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

// semantic relations plugin
#include "QmitkDataNodeUnlinkFromLesionAction.h"

// semantic relations module
#include <mitkNodePredicates.h>
#include <mitkSemanticRelationException.h>
#include <mitkUIDGeneratorBoost.h>

// mitk gui common plugin
#include <mitkDataNodeSelection.h>

// berry
#include <berryISelectionService.h>
#include <berryIWorkbenchPage.h>

// qt
#include <QMessageBox>

// namespace that contains the concrete action
namespace UnlinkFromLesionAction
{
  void Run(mitk::SemanticRelations* semanticRelations, const mitk::DataNode* dataNode)
  {
    if (nullptr == dataNode)
    {
      return;
    }

    if (mitk::NodePredicates::GetSegmentationPredicate()->CheckNode(dataNode))
    {
      UnlinkSegmentation(semanticRelations, dataNode);
    }
    else
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle("Could not unlink the selected data node.");
      msgBox.setText("Please chose a valid segmentation to unlink from its represented lesion!");
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.exec();
      return;
    }
  }

  void UnlinkSegmentation(mitk::SemanticRelations* semanticRelations, const mitk::DataNode* segmentation)
  {
    if (nullptr == segmentation)
    {
      return;
    }

    try
    {
      semanticRelations->UnlinkSegmentationFromLesion(segmentation);
    }
    catch (const mitk::SemanticRelationException& e)
    {
      std::stringstream exceptionMessage; exceptionMessage << e;
      QMessageBox msgBox;
      msgBox.setWindowTitle("Could not unlink the selected segmentation.");
      msgBox.setText("The program wasn't able to correctly unlink the selected segmentation.\n"
        "Reason:\n" + QString::fromStdString(exceptionMessage.str()));
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.exec();
      return;
    }
  }
}

QmitkDataNodeUnlinkFromLesionAction::QmitkDataNodeUnlinkFromLesionAction(QWidget* parent, berry::IWorkbenchPartSite::Pointer workbenchPartSite)
  : QAction(parent)
  , m_WorkbenchPartSite(workbenchPartSite)
{
  setText(tr("Unlink from lesion"));
  InitializeAction();
}

QmitkDataNodeUnlinkFromLesionAction::QmitkDataNodeUnlinkFromLesionAction(QWidget* parent, berry::IWorkbenchPartSite* workbenchPartSite)
  : QAction(parent)
  , m_WorkbenchPartSite(berry::IWorkbenchPartSite::Pointer(workbenchPartSite))
{
  setText(tr("Unlink from lesion"));
  InitializeAction();
}

QmitkDataNodeUnlinkFromLesionAction::~QmitkDataNodeUnlinkFromLesionAction()
{
  // nothing here
}

void QmitkDataNodeUnlinkFromLesionAction::SetDataStorage(mitk::DataStorage* dataStorage)
{
  if (m_DataStorage != dataStorage)
  {
    // set the new data storage
    m_DataStorage = dataStorage;
    m_SemanticRelations = std::make_unique<mitk::SemanticRelations>(m_DataStorage.Lock());
  }
}

void QmitkDataNodeUnlinkFromLesionAction::InitializeAction()
{
  connect(this, &QAction::triggered, this, &QmitkDataNodeUnlinkFromLesionAction::OnActionTriggered);
}

void QmitkDataNodeUnlinkFromLesionAction::OnActionTriggered(bool checked)
{
  if (nullptr == m_SemanticRelations)
  {
    return;
  }

  auto dataNode = GetSelectedNode();
  UnlinkFromLesionAction::Run(m_SemanticRelations.get(), dataNode);
}

QList<mitk::DataNode::Pointer> QmitkDataNodeUnlinkFromLesionAction::GetSelectedNodes()
{
  QList<mitk::DataNode::Pointer> selectedNodes;
  if (m_WorkbenchPartSite.Expired())
  {
    return selectedNodes;
  }

  berry::ISelection::ConstPointer selection = m_WorkbenchPartSite.Lock()->GetWorkbenchWindow()->GetSelectionService()->GetSelection();
  mitk::DataNodeSelection::ConstPointer currentSelection = selection.Cast<const mitk::DataNodeSelection>();

  if (currentSelection.IsNull() || currentSelection->IsEmpty())
  {
    return selectedNodes;
  }

  selectedNodes = QList<mitk::DataNode::Pointer>::fromStdList(currentSelection->GetSelectedDataNodes());
  return selectedNodes;
}

mitk::DataNode::Pointer QmitkDataNodeUnlinkFromLesionAction::GetSelectedNode()
{
  QList<mitk::DataNode::Pointer> selectedNodes = GetSelectedNodes();
  if (selectedNodes.empty())
  {
    return nullptr;
  }

  // no batch action; should only be called with a single node
  mitk::DataNode::Pointer dataNode = selectedNodes.front();
  if (nullptr == dataNode)
  {
    return nullptr;
  }

  return dataNode;
}
