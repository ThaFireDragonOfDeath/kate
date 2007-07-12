/* This file is part of the KDE libraries
   Copyright (C) 2005-2006 Hamish Rodda <rodda@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "katecompletionwidget.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QHeaderView>
#include <QtCore/QTimer>
#include <QtGui/QLabel>
#include <QtGui/QToolButton>
#include <QtGui/QSizeGrip>
#include <QtGui/QPushButton>

#include <kicon.h>
#include <kdialog.h>

#include <ktexteditor/cursorfeedback.h>

#include "kateview.h"
#include "katesmartmanager.h"
#include "katerenderer.h"
#include "kateconfig.h"
#include "katedocument.h"
#include "katesmartrange.h"
#include "kateedit.h"

#include "katecompletionmodel.h"
#include "katecompletiontree.h"
#include "katecompletionconfig.h"

//#include "modeltest.h"

KateCompletionWidget::KateCompletionWidget(KateView* parent)
  : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint)
  , m_presentationModel(new KateCompletionModel(this))
  , m_completionRange(0L)
  , m_automaticInvocation(false)
  , m_filterInstalled(false)
{
  //new ModelTest(m_presentationModel, this);

  setFrameStyle( QFrame::Box | QFrame::Plain );
  setLineWidth( 1 );
  //setWindowOpacity(0.8);

  m_entryList = new KateCompletionTree(this);
  m_entryList->setModel(m_presentationModel);

  m_statusBar = new QWidget(this);
  m_statusBar->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );

  m_sortButton = new QToolButton(m_statusBar);
  m_sortButton->setIcon(KIcon("sort"));
  m_sortButton->setCheckable(true);
  connect(m_sortButton, SIGNAL(toggled(bool)), m_presentationModel, SLOT(setSortingEnabled(bool)));

  m_sortText = new QLabel(i18n("Sort: None"), m_statusBar);

  m_filterButton = new QToolButton(m_statusBar);
  m_filterButton->setIcon(KIcon("search-filter"));
  m_filterButton->setCheckable(true);
  connect(m_filterButton, SIGNAL(toggled(bool)), m_presentationModel, SLOT(setFilteringEnabled(bool)));

  m_filterText = new QLabel(i18n("Filter: None"), m_statusBar);

  m_configButton = new QPushButton(KIcon("configure"), i18n("Setup"), m_statusBar);
  connect(m_configButton, SIGNAL(pressed()), SLOT(showConfig()));

  QSizeGrip* sg = new QSizeGrip(m_statusBar);

  QHBoxLayout* statusLayout = new QHBoxLayout(m_statusBar);
  statusLayout->addWidget(m_sortButton);
  statusLayout->addWidget(m_sortText);
  statusLayout->addSpacing(8);
  statusLayout->addWidget(m_filterButton);
  statusLayout->addWidget(m_filterText);
  statusLayout->addSpacing(8);
  statusLayout->addStretch();
  QVBoxLayout* gripLayout = new QVBoxLayout();
  gripLayout->addStretch();
  statusLayout->addWidget(m_configButton);
  gripLayout->addWidget(sg);
  statusLayout->addLayout(gripLayout);
  statusLayout->setMargin(0);
  statusLayout->setSpacing(2);

  QVBoxLayout* vl = new QVBoxLayout(this);
  vl->addWidget(m_entryList);
  vl->addWidget(m_statusBar);
  vl->setMargin(0);

  // Keep branches expanded
  connect(m_presentationModel, SIGNAL(modelReset()), this, SLOT(modelReset()), Qt::QueuedConnection);
  connect(m_presentationModel, SIGNAL(rowsInserted(const QModelIndex&, int, int)), SLOT(rowsInserted(const QModelIndex&, int, int)));

  connect(view(), SIGNAL(cursorPositionChanged(KTextEditor::View*, const KTextEditor::Cursor&)), SLOT(cursorPositionChanged()));
  connect(view()->doc()->history(), SIGNAL(editDone(KateEditInfo*)), SLOT(editDone(KateEditInfo*)));

  // This is a non-focus widget, it is passed keyboard input from the view
  setFocusPolicy(Qt::NoFocus);
  foreach (QWidget* childWidget, findChildren<QWidget*>())
    childWidget->setFocusPolicy(Qt::NoFocus);
}

const KateCompletionModel* KateCompletionWidget::model() const {
  return m_presentationModel;
}

KateCompletionModel* KateCompletionWidget::model() {
  return m_presentationModel;
}

void KateCompletionWidget::rowsInserted(const QModelIndex& parent, int rowFrom, int rowEnd)
{
  for (int i = rowFrom; i <= rowEnd; ++i)
    m_entryList->expand(m_presentationModel->index(i, 0, parent));
}

KateView * KateCompletionWidget::view( ) const
{
  return static_cast<KateView*>(const_cast<QObject*>(parent()));
}

void KateCompletionWidget::startCompletion( const KTextEditor::Range & word, KTextEditor::CodeCompletionModel * model, KTextEditor::CodeCompletionModel::InvocationType invocationType)
{
  if (!word.isValid()) {
    kWarning(13035) << k_funcinfo << "Invalid range given to start code completion!" << endl;
    return;
  }

  kDebug(13035) << k_funcinfo << word << " " << model << endl;

  if (!m_filterInstalled) {
    if (!QApplication::activeWindow()) {
      kWarning(13035) << k_funcinfo << "No active window to install event filter on!!" << endl;
      return;
    }
    // Enable the cc box to move when the editor window is moved
    QApplication::activeWindow()->installEventFilter(this);
    m_filterInstalled = true;
  }

  if (isCompletionActive())
    abortCompletion();

  m_completionRange = view()->doc()->smartManager()->newSmartRange(word);
  m_completionRange->setInsertBehavior(KTextEditor::SmartRange::ExpandRight);

  connect(m_completionRange->smartStart().notifier(), SIGNAL(characterDeleted(KTextEditor::SmartCursor*, bool)), SLOT(startCharacterDeleted(KTextEditor::SmartCursor*, bool)));

  cursorPositionChanged();

  if (model)
    m_presentationModel->setCompletionModel(model);
  else
    m_presentationModel->setCompletionModels(m_sourceModels);

  updatePosition(true);

  if (!m_presentationModel->completionModels().isEmpty()) {
    show();

    foreach (KTextEditor::CodeCompletionModel* model, m_presentationModel->completionModels()) {
      model->completionInvoked(view(), word, invocationType);
      kDebug(13035) << "CC Model Statistics: " << model << endl << "  Completions: " << model->rowCount(QModelIndex()) << endl;
    }

    m_entryList->resizeColumns(false, true);

    m_presentationModel->setCurrentCompletion(view()->doc()->text(KTextEditor::Range(m_completionRange->start(), view()->cursorPosition())));
  }
}

void KateCompletionWidget::updatePosition(bool force)
{
  if (!force && !isCompletionActive())
    return;

  QPoint cursorPosition = view()->cursorToCoordinate(m_completionRange->start());
  if (cursorPosition == QPoint(-1,-1))
    // Start of completion range is now off-screen -> abort
    return abortCompletion();

  QPoint p = view()->mapToGlobal( cursorPosition );
  int x = p.x() - m_entryList->header()->sectionPosition(m_entryList->header()->visualIndex(m_presentationModel->translateColumn(KTextEditor::CodeCompletionModel::Name))) - 2;
  int y = p.y();
  if ( y + height() + view()->renderer()->config()->fontMetrics().height() > QApplication::desktop()->height() )
    y -= height();
  else
    y += view()->renderer()->config()->fontMetrics().height();

  if (x + width() > QApplication::desktop()->width())
    x = QApplication::desktop()->width() - width();

  move( QPoint(x,y) );
}

void KateCompletionWidget::cursorPositionChanged( )
{
  if (!isCompletionActive())  {
    m_presentationModel->setCurrentCompletion(QString());
    return;
  }

  KTextEditor::Cursor cursor = view()->cursorPosition();

  if (m_completionRange->start() > cursor || m_completionRange->end() < cursor)
    return abortCompletion();

  QString currentCompletion = view()->doc()->text(KTextEditor::Range(m_completionRange->start(), view()->cursorPosition()));

  // FIXME - allow client to specify this?
  static QRegExp allowedText("^(\\w*)");
  if (!allowedText.exactMatch(currentCompletion))
    return abortCompletion();

  m_presentationModel->setCurrentCompletion(currentCompletion);
}

bool KateCompletionWidget::isCompletionActive( ) const
{
  bool ret = !isHidden();
  if (ret)
    Q_ASSERT(m_completionRange && m_completionRange->isValid());

  return ret;
}

void KateCompletionWidget::abortCompletion( )
{
  kDebug(13035) << k_funcinfo << endl;

  if (!isCompletionActive())
    return;

  hide();

  m_presentationModel->clearCompletionModels();

  delete m_completionRange;
  m_completionRange = 0L;

  view()->sendCompletionAborted();
}

void KateCompletionWidget::execute()
{
  kDebug(13035) << k_funcinfo << endl;

  if (!isCompletionActive())
    return;

  QModelIndex toExecute = m_entryList->selectionModel()->currentIndex();

  if (!toExecute.isValid())
    return abortCompletion();

  toExecute = m_presentationModel->mapToSource(toExecute);
  KTextEditor::Cursor start = m_completionRange->start();

  // encapsulate all editing as being from the code completion, and undo-able in one step.
  view()->doc()->editStart(true, Kate::CodeCompletionEdit);

  KTextEditor::CodeCompletionModel* model = static_cast<KTextEditor::CodeCompletionModel*>(const_cast<QAbstractItemModel*>(toExecute.model()));
  model->executeCompletionItem(view()->document(), *m_completionRange, toExecute.row());

  view()->doc()->editEnd();

  hide();

  view()->sendCompletionExecuted(start, model, toExecute.row());
}

void KateCompletionWidget::resizeEvent( QResizeEvent * event )
{
  QWidget::resizeEvent(event);

  m_entryList->resizeColumns(true);
}

void KateCompletionWidget::hideEvent( QHideEvent * event )
{
  QWidget::hideEvent(event);

  if (isCompletionActive())
    abortCompletion();
}

KateSmartRange * KateCompletionWidget::completionRange( ) const
{
  return m_completionRange;
}

void KateCompletionWidget::modelReset( )
{
  m_entryList->expandAll();
}

KateCompletionTree* KateCompletionWidget::treeView() const {
  return m_entryList;
}

bool KateCompletionWidget::canExpandCurrentItem() const {
  if( !m_entryList->currentIndex().isValid() ) return false;
  return model()->isExpandable( m_entryList->currentIndex() ) && !model()->isExpanded( m_entryList->currentIndex().row() );
}

bool KateCompletionWidget::canCollapseCurrentItem() const {
  if( !m_entryList->currentIndex().isValid() ) return false;
  return model()->isExpandable( m_entryList->currentIndex() ) && model()->isExpanded( m_entryList->currentIndex().row() );
}

void KateCompletionWidget::setCurrentItemExpanded( bool expanded ) {
  if( !m_entryList->currentIndex().isValid() ) return;
  model()->setExpanded(m_entryList->currentIndex(), expanded);
}

void KateCompletionWidget::startCharacterDeleted( KTextEditor::SmartCursor*, bool deletedBefore )
{
  if (deletedBefore)
    // Cannot abort completion from this function, or the cursor will be deleted before returning
    QTimer::singleShot(0, this, SLOT(abortCompletion()));
}

bool KateCompletionWidget::eventFilter( QObject * watched, QEvent * event )
{
  bool ret = QFrame::eventFilter(watched, event);

  if (watched != this)
    if (event->type() == QEvent::Move)
      updatePosition();

  return ret;
}

void KateCompletionWidget::nextCompletion( )
{
  m_entryList->nextCompletion();
}

void KateCompletionWidget::previousCompletion( )
{
  m_entryList->previousCompletion();
}

void KateCompletionWidget::pageDown( )
{
  m_entryList->pageDown();
}

void KateCompletionWidget::pageUp( )
{
  m_entryList->pageUp();
}

void KateCompletionWidget::top( )
{
  m_entryList->top();
}

void KateCompletionWidget::bottom( )
{
  m_entryList->bottom();
}

void KateCompletionWidget::showConfig( )
{
  abortCompletion();

  KateCompletionConfig* config = new KateCompletionConfig(m_presentationModel, view());
  config->exec();
  delete config;
}

void KateCompletionWidget::registerCompletionModel(KTextEditor::CodeCompletionModel* model)
{
  m_sourceModels.append(model);

  if (isCompletionActive()) {
    m_presentationModel->addCompletionModel(model);
  }
}

void KateCompletionWidget::unregisterCompletionModel(KTextEditor::CodeCompletionModel* model)
{
  m_sourceModels.removeAll(model);
}

bool KateCompletionWidget::isAutomaticInvocationEnabled() const
{
  return m_automaticInvocation;
}

void KateCompletionWidget::setAutomaticInvocationEnabled(bool enabled)
{
  m_automaticInvocation = enabled;
}

void KateCompletionWidget::editDone(KateEditInfo * edit)
{
  if (isCompletionActive())
    return;

  if (!isAutomaticInvocationEnabled())
    return;

  if (edit->editSource() != Kate::UserInputEdit)
    return;

  if (edit->isRemoval())
    return;

  if (edit->newText().isEmpty())
    return;

  QString lastLine = edit->newText().last();

  if (lastLine.isEmpty())
    return;

  QChar lastChar = lastLine.at(lastLine.count() - 1);

  if (lastChar.isLetter() || lastChar.isNumber() || lastChar == '.' || lastChar == '_' || lastChar == '>') {
    // Start automatic code completion
    KTextEditor::Range range = determineRange();
    if (range.isValid())
      startCompletion(range, 0, KTextEditor::CodeCompletionModel::AutomaticInvocation);
    else
      kWarning(13035) << k_funcinfo << "Completion range was invalid even though it was expected to be valid." << endl;
  }
}

void KateCompletionWidget::userInvokedCompletion()
{
  startCompletion(determineRange(), 0, KTextEditor::CodeCompletionModel::UserInvocation);
}

KTextEditor::Range KateCompletionWidget::determineRange() const
{
  KTextEditor::Cursor end = view()->cursorPosition();

  // the end cursor should always be valid, otherwise things go wrong
  // Assumption: view()->cursorPosition() is always valid.
  Q_ASSERT(end.isValid());

  QString text = view()->document()->line(end.line());

  static QRegExp findWordStart( "\\b([_\\w]+)$" );
  static QRegExp findWordEnd( "^([_\\w]*)\\b" );

  KTextEditor::Cursor start = end;

  if (findWordStart.lastIndexIn(text.left(end.column())) >= 0)
    start.setColumn(findWordStart.pos(1));

  if (findWordEnd.indexIn(text.mid(end.column())) >= 0)
    end.setColumn(end.column() + findWordEnd.cap(1).length());

  return KTextEditor::Range(start, end);
}

#include "katecompletionwidget.moc"

// kate: space-indent on; indent-width 2; replace-tabs on;
