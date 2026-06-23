# -*- coding: utf-8 -*-
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

fig, ax = plt.subplots(figsize=(20, 7))
ax.set_xlim(0, 20)
ax.set_ylim(3.5, 10.2)
ax.axis('off')
fig.patch.set_facecolor('white')

C_QT  = '#D6EAF8'; E_QT  = '#1A5276'; LW_QT  = 2.0
C_APP = '#EAFAF1'; E_APP = '#1E8449'; LW_APP = 1.4
LINE  = '#444444'

BW = 2.5    # box width
BH = 0.52   # box height

def box(cx, cy, label, qt=False):
    fc, ec, lw = (C_QT, E_QT, LW_QT) if qt else (C_APP, E_APP, LW_APP)
    ax.add_patch(mpatches.FancyBboxPatch(
        (cx - BW/2, cy - BH/2), BW, BH,
        boxstyle='round,pad=0.07',
        facecolor=fc, edgecolor=ec, linewidth=lw, zorder=3))
    ax.text(cx, cy, label, ha='center', va='center', zorder=4, color='#0D0D0D',
            fontsize=9.5 if qt else 8.8,
            fontweight='bold' if qt else 'normal',
            fontfamily='monospace')

def vline(x, y0, y1):
    ax.plot([x, x], [y0, y1], color=LINE, lw=1.2, zorder=2)

def hline(x0, x1, y):
    ax.plot([x0, x1], [y, y], color=LINE, lw=1.2, zorder=2)

def tip(x, y_tip):
    ax.annotate('', xy=(x, y_tip), xytext=(x, y_tip - 0.001),
                arrowprops=dict(arrowstyle='-|>', color=LINE,
                                lw=1.2, mutation_scale=13), zorder=2)

# ── y levels ────────────────────────────────────────────────────────────────
ROOT_Y  = 9.3   # base class row
BUS1_Y  = 8.1   # bus for row-1 children
ROW1_Y  = 7.1   # row-1 children
BUS2_Y  = 5.95  # bus for row-2 children
ROW2_Y  = 4.95  # row-2 children

# ════════════════════════════════════════════════════════════════════════════
# TREE 1 — QMainWindow
# ════════════════════════════════════════════════════════════════════════════
QMW_X = 1.5
box(QMW_X, ROOT_Y, 'QMainWindow', qt=True)
box(QMW_X, ROW1_Y, 'MainWindow')
vline(QMW_X, ROW1_Y + BH/2, ROOT_Y - BH/2)
tip(QMW_X, ROOT_Y - BH/2)

# ════════════════════════════════════════════════════════════════════════════
# TREE 2 — QWidget  (8 children in 2 rows of 4)
# ════════════════════════════════════════════════════════════════════════════
QW_X   = 8.4     # root centre
STRIDE = 2.5     # spacing between child columns

# 4 column centres, symmetric around QW_X
xs = [QW_X - 1.5*STRIDE + i*STRIDE for i in range(4)]
# → [4.65, 7.15, 9.65, 12.15]

row1 = ['ClientsView', 'AppointmentsView', 'StaffView', 'ProvidersView']
row2 = ['StockView',   'RecordDetailsView', 'StaffDetailsView', 'QuickAppointmentView']

box(QW_X, ROOT_Y, 'QWidget', qt=True)

# trunk from root bottom to bus2 bottom
vline(QW_X, ROOT_Y - BH/2, BUS2_Y)

# horizontal bus-1
hline(xs[0], xs[-1], BUS1_Y)

# horizontal bus-2
hline(xs[0], xs[-1], BUS2_Y)

for cx in xs:
    # bus-1 drop to row-1
    vline(cx, BUS1_Y, ROW1_Y + BH/2)
    tip(cx, ROW1_Y + BH/2)
    # bus-2 drop to row-2
    vline(cx, BUS2_Y, ROW2_Y + BH/2)
    tip(cx, ROW2_Y + BH/2)

for cx, lbl in zip(xs, row1):
    box(cx, ROW1_Y, lbl)
for cx, lbl in zip(xs, row2):
    box(cx, ROW2_Y, lbl)

# ════════════════════════════════════════════════════════════════════════════
# TREE 3 — QStyledItemDelegate
# ════════════════════════════════════════════════════════════════════════════
QSD_X = 17.0   # root centre  (right side; xlim=20)
D_XS  = [15.75, 18.25]
D_BUS = BUS1_Y

box(QSD_X, ROOT_Y, 'QStyledItemDelegate', qt=True)

vline(QSD_X, ROOT_Y - BH/2, D_BUS)
hline(D_XS[0], D_XS[1], D_BUS)

for cx, lbl in zip(D_XS, ['ClientNameDelegate', 'StaffNameDelegate']):
    vline(cx, D_BUS, ROW1_Y + BH/2)
    tip(cx, ROW1_Y + BH/2)
    box(cx, ROW1_Y, lbl)

# ════════════════════════════════════════════════════════════════════════════
# Legend
# ════════════════════════════════════════════════════════════════════════════
handles = [
    mpatches.Patch(facecolor=C_QT, edgecolor=E_QT, lw=2, label='Базовий клас Qt'),
    mpatches.Patch(facecolor=C_APP, edgecolor=E_APP, lw=1.4, label='Клас застосунку'),
]
ax.legend(handles=handles, loc='lower center', fontsize=10,
          frameon=True, ncol=2, bbox_to_anchor=(0.5, 0.01))

out = r'C:\Users\Lenovo\Documents\GitHub\OOP-Coursework\uml_hierarchy.png'
plt.savefig(out, dpi=200, bbox_inches='tight', facecolor='white')
print(f'Saved: {out}')
