/****************************************************************************
** Nestor: ordenar una tabla con clic en el titulo (ver pvtablesort.h)
****************************************************************************/
#include "pvtablesort.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QPersistentModelIndex>
#include <QSignalBlocker>
#include <QCollator>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QAbstractButton>
#include <QLineEdit>
#include <QSet>
#include <algorithm>

namespace {

class TableSorter : public QObject
{
public:
  TableSorter(QTableWidget *t) : QObject(t), table(t) { setObjectName("pvbTableSorter"); }
  QTableWidget *table;
  int column = -1;
  bool ascending = true;
  QList<QPersistentModelIndex> stamp;   // el orden del archivo (filas que existian al ordenar)
};

TableSorter *sorterOf(QTableWidget *t)
{
  // sin Q_OBJECT (no hace falta moc): se busca por el nombre, que solo usa TableSorter
  return t ? static_cast<TableSorter *>(t->findChild<QObject *>("pvbTableSorter", Qt::FindDirectChildrenOnly)) : nullptr;
}

// el valor de un widget de celda (o de uno de adentro: una casilla centrada en un QWidget)
bool widgetKey(QWidget *w, QString *key)
{
  if(QComboBox *c = qobject_cast<QComboBox *>(w)) { *key = c->currentText(); return true; }
  if(QSpinBox *s = qobject_cast<QSpinBox *>(w)) { *key = QString::number(s->value()); return true; }
  if(QDoubleSpinBox *s = qobject_cast<QDoubleSpinBox *>(w)) { *key = QString::number(s->value(), 'g', 15); return true; }
  if(QLineEdit *e = qobject_cast<QLineEdit *>(w)) { *key = e->text(); return true; }
  if(QAbstractButton *b = qobject_cast<QAbstractButton *>(w))
  {
    *key = b->isCheckable() ? QString(b->isChecked() ? "1" : "0") : b->text();
    return true;
  }
  for(QWidget *child : w->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
    if(widgetKey(child, key)) return true;
  return false;
}

QString cellKey(QTableWidget *t, int r, int c)
{
  QString key;
  if(QWidget *w = t->cellWidget(r, c)) { if(widgetKey(w, &key)) return key; }
  QTableWidgetItem *i = t->item(r, c);
  if(i == nullptr) return QString();
  key = i->text();
  if(key.isEmpty() && i->data(Qt::CheckStateRole).isValid()) key = (i->checkState() == Qt::Checked) ? "1" : "0";
  return key;
}

bool number(const QString &s, double *v)
{
  bool ok;
  QString x = s.trimmed();
  *v = x.toDouble(&ok);
  if(!ok && x.count(',') == 1 && !x.contains('.')) *v = QString(x).replace(',', '.').toDouble(&ok);   // 1,5
  return ok;
}

// filas visibles en el orden del archivo: las del sello que siguen existiendo, despues las nuevas
QList<int> fileOrder(TableSorter *s)
{
  QTableWidget *t = s->table;
  QList<int> out;
  QSet<int> seen;
  for(const QPersistentModelIndex &p : s->stamp)
    if(p.isValid() && p.row() < t->rowCount() && !seen.contains(p.row())) { out << p.row(); seen.insert(p.row()); }
  for(int r=0; r<t->rowCount(); r++) if(!seen.contains(r)) out << r;
  return out;
}

// las filas quedan en el orden 'perm' (indices visibles). Una columna auxiliar con la posicion:
// sortItems mueve tambien los widgets de celda y la seleccion
void reorder(QTableWidget *t, const QList<int> &perm)
{
  bool same = true;
  for(int i=0; i<perm.size() && same; i++) same = (perm[i] == i);
  if(same) return;
  const QSignalBlocker blocker(t);
  const bool sorting = t->isSortingEnabled();
  if(sorting) t->setSortingEnabled(false);
  QHeaderView *h = t->horizontalHeader();
  const bool stretch = h->stretchLastSection();
  if(stretch) h->setStretchLastSection(false);
  const int aux = t->columnCount();
  t->insertColumn(aux);
  for(int i=0; i<perm.size(); i++) t->setItem(perm[i], aux, new QTableWidgetItem(QString("%1").arg(i, 9, 10, QChar('0'))));
  t->sortItems(aux, Qt::AscendingOrder);
  t->removeColumn(aux);
  if(stretch) h->setStretchLastSection(true);
  if(sorting) t->setSortingEnabled(true);
}

void sortNow(TableSorter *s)
{
  QTableWidget *t = s->table;
  if(s->column < 0 || s->column >= t->columnCount()) return;
  // el sello: el orden del archivo antes de este orden (las filas nuevas, al final)
  QList<QPersistentModelIndex> stamp;
  for(int r : fileOrder(s)) stamp << QPersistentModelIndex(t->model()->index(r, 0));
  s->stamp = stamp;

  const int n = t->rowCount(), c = s->column;
  QStringList keys;
  for(int r=0; r<n; r++) keys << cellKey(t, r, c);
  // con el idioma C (LANG=C o C.UTF-8, comun en Linux) QCollator compara texto plano e ignora el modo numerico
  QCollator coll(QLocale().language() == QLocale::C ? QLocale(QLocale::English) : QLocale());
  coll.setNumericMode(true);
  coll.setCaseSensitivity(Qt::CaseInsensitive);
  QList<int> perm;
  for(int r=0; r<n; r++) perm << r;
  // vacias al final (en los dos sentidos); numeros como numeros; textos con QCollator
  auto less = [&](int a, int b) {
    const QString &ka = keys[a], &kb = keys[b];
    if(ka.isEmpty() || kb.isEmpty()) return !ka.isEmpty() && kb.isEmpty();
    double va, vb;
    int cmp;
    if(number(ka, &va) && number(kb, &vb)) cmp = (va < vb) ? -1 : (va > vb) ? 1 : 0;
    else cmp = coll.compare(ka, kb);
    return s->ascending ? cmp < 0 : cmp > 0;
  };
  std::stable_sort(perm.begin(), perm.end(), less);
  reorder(t, perm);
  t->horizontalHeader()->setSortIndicatorShown(true);
  t->horizontalHeader()->setSortIndicator(c, s->ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
}

} // namespace

void pvbSortableTable(QTableWidget *t)
{
  if(t == nullptr || sorterOf(t) != nullptr) return;
  TableSorter *s = new TableSorter(t);
  QHeaderView *h = t->horizontalHeader();
  h->setSectionsClickable(true);
  if(h->toolTip().isEmpty()) h->setToolTip(QObject::tr("Clic en el titulo de una columna: ordenar (otro clic: al reves)"));
  QObject::connect(h, &QHeaderView::sectionClicked, s, [s](int column) {
    if(s->table->isSortingEnabled()) return;   // la tabla ya ordena sola
    if(column == s->column) s->ascending = !s->ascending;
    else { s->column = column; s->ascending = true; }
    sortNow(s);
  });
}

QList<int> pvbTableOrder(QTableWidget *t)
{
  TableSorter *s = sorterOf(t);
  if(s != nullptr) return fileOrder(s);
  QList<int> out;
  for(int r=0; t && r<t->rowCount(); r++) out << r;
  return out;
}

void pvbTableAcceptOrder(QTableWidget *t)
{
  TableSorter *s = sorterOf(t);
  if(s == nullptr) return;
  s->stamp.clear();
  s->column = -1;
  t->horizontalHeader()->setSortIndicatorShown(false);
}

void pvbTableResort(QTableWidget *t)
{
  TableSorter *s = sorterOf(t);
  if(s != nullptr && s->column >= 0) sortNow(s);
}

void pvbTableSortBy(QTableWidget *t, int column, bool ascending)
{
  pvbSortableTable(t);
  TableSorter *s = sorterOf(t);
  s->column = column;
  s->ascending = ascending;
  sortNow(s);
}
