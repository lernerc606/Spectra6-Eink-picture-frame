import sys
import os
import numpy as np
import tkinter as tk
from tkinter import filedialog
from PIL import Image, ImageTk

###################################
# Disable Windows DPI scaling
###################################
try:
    from ctypes import windll
    windll.shcore.SetProcessDpiAwareness(1)
except:
    pass

###################################
# CONFIG
###################################
TARGET_WIDTH  = 1200
TARGET_HEIGHT = 1600
# rename this to reflect orientation, and default to portrait:
is_landscape = False

DESIRED_W, DESIRED_H = 2454, 1908

DEFAULT_DITHER_PALETTE = [
    [30, 30, 30],
    [195,255,255],
    [255,255,0],
    [255,70,70],
    [50,50,255],
    [80,140,80]
]
DEFAULT_EINK_PALETTE = [
    [0,0,0],
    [140,140,110],
    [140,120,0],
    [100,15,15],
    [40,40,100],
    [40,63,41]
]
RAW_MAP = {0:0x0,1:0x1,2:0x2,3:0x3,4:0x5,5:0x6}

# Will hold Entry widgets for palette values
dith_palette_entries  = [[None]*3 for _ in range(6)]
eink_palette_entries = [[None]*3 for _ in range(6)]

###################################
# PIPELINE FUNCTIONS
###################################
def fit_window_to_image():
    # make sure all widget sizes are up to date
    root.update_idletasks()

    # 1) choose pane dimensions by orientation
    if is_landscape:
        pane_w, pane_h = TARGET_HEIGHT, TARGET_WIDTH   # 1600×1200
    else:
        pane_w, pane_h = TARGET_WIDTH, TARGET_HEIGHT   # 1200×1600

    # 2) padding around the panes (grid padx/pady + top frame pady)
    pad_x = 5 * 4   # left/right on left & right pane
    pad_y = 5 * 2   # top/bottom on panes
    top_pady = top.pack_info().get("pady", 5)
    pad_y += top_pady * 2

    # 3) scrollbar thickness
    vbar_w = left_pane.vbar.winfo_reqwidth()
    hbar_h = left_pane.hbar.winfo_reqheight()

    # 4) canvas border & highlight
    bd = int(left_pane.canvas.cget("bd"))
    ht = int(left_pane.canvas.cget("highlightthickness"))
    extra_x = 2 * (bd + ht)
    extra_y = 2 * (bd + ht)

    # 5) a tiny fudge factor
    fudge = 4

    # 6) control-bar height
    ctrl_h = top.winfo_height()

    # 7) compute “ideal” window size
    new_w = pane_w * 2 + pad_x + vbar_w + extra_x + fudge
    new_h = ctrl_h + pane_h + pad_y + hbar_h + extra_y + fudge

    # 8) compare to actual screen size
    sw, sh = root.winfo_screenwidth(), root.winfo_screenheight()
    if new_w > sw or new_h > sh:
        # doesn’t fit → maximize
        root.state('zoomed')
    else:
        # fits → normal + set exact geometry
        root.state('normal')
        root.geometry(f"{new_w}x{new_h}+50+50")

    # 9) finally, resize the canvases themselves
    left_pane.canvas.config(width=pane_w, height=pane_h)
    right_pane.canvas.config(width=pane_w, height=pane_h)


def scale_to_target(img):
    # choose target dims based on orientation
    if is_landscape:
        tw, th = TARGET_HEIGHT, TARGET_WIDTH    # 1600×1200
    else:
        tw, th = TARGET_WIDTH, TARGET_HEIGHT    # 1200×1600

    w, h = img.size
    sc = min(tw/w, th/h)
    nw, nh = int(w*sc), int(h*sc)
    if sc != 1:
        img = img.resize((nw, nh), Image.LANCZOS)

    # center‐letterbox onto a tw×th canvas
    canvas = Image.new("RGB", (tw, th), (0,0,0))
    offset = ((tw-nw)//2, (th-nh)//2)
    canvas.paste(img, offset)
    return canvas

def apply_inverse_gamma(img, inv_g):
    if inv_g <= 0: inv_g = 1.0
    gamma_val = 1.0 / inv_g
    arr = np.asarray(img, dtype=np.float32)/255.0
    arr = arr ** gamma_val
    arr = (arr*255.0).clip(0,255).astype(np.uint8)
    return Image.fromarray(arr)

def apply_brightness_contrast(img, brightness, contrast):
    arr = np.asarray(img, dtype=np.float32)
    arr = (arr-128.0)*(contrast/100.0)+128.0 + brightness
    arr = np.clip(arr,0,255).astype(np.uint8)
    return Image.fromarray(arr)

def apply_rgb_adjustments(img, r_pct, g_pct, b_pct):
    arr = np.asarray(img, dtype=np.float32)
    arr[...,0] *= (r_pct/100.0)
    arr[...,1] *= (g_pct/100.0)
    arr[...,2] *= (b_pct/100.0)
    arr = np.clip(arr,0,255).astype(np.uint8)
    return Image.fromarray(arr)


def _get_entry_int(entry):
    try:
        v = int(float(entry.get()))
    except:
        v = 0
    return max(0, min(255, v))

def build_dithering_palette():
    pal = []
    for i in range(6):
        r = _get_entry_int(dith_palette_entries[i][0])
        g = _get_entry_int(dith_palette_entries[i][1])
        b = _get_entry_int(dith_palette_entries[i][2])
        pal.append([r,g,b])
    return pal

def build_eink_palette():
    pal = []
    for i in range(6):
        r = _get_entry_int(eink_palette_entries[i][0])
        g = _get_entry_int(eink_palette_entries[i][1])
        b = _get_entry_int(eink_palette_entries[i][2])
        pal.append([r,g,b])
    return pal

def full_dither_pipeline(original, inv_g, bright, contr, r_pct, g_pct, b_pct):
    # 1) start by building up `pipe`
    pipe = apply_inverse_gamma(original, inv_g)
    pipe = apply_brightness_contrast(pipe, bright, contr)
    pipe = apply_rgb_adjustments(  pipe, r_pct, g_pct, b_pct)

    # 2) now scale to the correct portrait/landscape target
    pipe = scale_to_target(pipe)

    # 3) create your 8-bit indexed image
    dith_pal = build_dithering_palette()
    pal_img  = Image.new("P",(1,1))
    flat     = sum(dith_pal, []) + [0]*(768 - 3*6)
    pal_img.putpalette(flat)
    d8  = pipe.quantize(palette=pal_img, dither=True)
    idx = np.array(d8.convert("P"))

    # 4) map indices → RGB
    w,h = pipe.size
    out = np.zeros((h, w, 3), dtype=np.uint8)
    for i, col in enumerate(build_eink_palette()):
        out[idx==i] = col

    return Image.fromarray(out,"RGB"), idx

###################################
# SCROLLABLE IMAGE CANVAS CLASS
###################################
class ScrollableImage(tk.Frame):
    def __init__(self, parent, w, h):
        super().__init__(parent)
        self.canvas = tk.Canvas(self, width=w, height=h, bg="gray")
        self.vbar   = tk.Scrollbar(self, orient="vertical",
                                   command=self.canvas.yview)
        self.hbar   = tk.Scrollbar(self, orient="horizontal",
                                   command=self.canvas.xview)
        self.canvas.configure(xscrollcommand=self.hbar.set,
                              yscrollcommand=self.vbar.set)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        self.vbar.grid( row=0, column=1, sticky="ns")
        self.hbar.grid( row=1, column=0, sticky="ew")
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)
        self._img_id = None

    def show(self, pil_img):
        self._tk = ImageTk.PhotoImage(pil_img)
        if self._img_id is None:
            self._img_id = self.canvas.create_image(0,0,
                                                    anchor="nw",
                                                    image=self._tk)
        else:
            self.canvas.itemconfigure(self._img_id,
                                      image=self._tk)
        w,h = pil_img.size
        self.canvas.config(scrollregion=(0,0,w,h))

###################################
# SLIDER ↔ ENTRY HELPERS
###################################
def slider_to_field(slider, entry):
    entry.delete(0, tk.END)
    entry.insert(0, str(slider.get()))
    realtime_dither()

def field_to_slider(entry, slider):
    try:
        val = float(entry.get())
    except:
        return
    slider.set(val)
    realtime_dither()

def bind_slider_and_entry(slider, entry):
    slider.config(command=lambda val: slider_to_field(slider, entry))
    entry.bind("<KeyRelease>",
               lambda e: field_to_slider(entry, slider))

def read_val(slider, entry):
    try:
        return float(entry.get())
    except:
        return slider.get()
    
is_flipped = False


def toggle_layout():
    global is_landscape, pane_w, pane_h

    # flip our orientation flag
    is_landscape = not is_landscape
    # resize window & panes for the new orientation:
    fit_window_to_image()

    # swap the pane dimensions
    pane_w, pane_h = pane_h, pane_w
    left_pane.canvas.config(width=pane_w, height=pane_h)
    right_pane.canvas.config(width=pane_w, height=pane_h)

    # re-draw both sides in the new orientation
    update_left()
    realtime_dither()


###################################
# MAIN WINDOW
###################################
root = tk.Tk()
root.title("Adaptive E-Ink Dither Tool")
root.call("tk","scaling", 1)

# clamp to screen size
sw, sh = root.winfo_screenwidth(), root.winfo_screenheight()
win_w = min(DESIRED_W, sw - 100)
win_h = min(DESIRED_H, sh - 100)
root.geometry(f"{win_w}x{win_h}+50+50")
root.minsize(800,600)
root.resizable(True, True)
root.option_add("*Font","Helvetica 20")

###################################
# TOP FRAME: CONTROLS
###################################
top = tk.Frame(root, height=300, bg="lightblue")
top.pack(fill="x", padx=5, pady=5)

# Column 0: InvGamma, Brightness, Contrast
col0 = tk.Frame(top, bg="lightblue"); col0.pack(side="left", padx=5)
for label, base, lo, hi, step, init in [
    ("InvGamma", "inv_gamma", 0.5,   3.0, 0.1, 1.0),
    ("Brightness","brightness",-100,100,   1, 0  ),
    ("Contrast",  "contrast",50,  150,   1,100),
]:
    f = tk.Frame(col0, bg="lightblue"); f.pack(pady=5)
    tk.Label(f, text=f"{label}:").pack(side="left")
    ent = tk.Entry(f, width=5); ent.insert(0,str(init)); ent.pack(side="left", padx=2)
    sld = tk.Scale(f,
                   from_=lo, to=hi,
                   resolution=step,
                   length=150,
                   orient="horizontal",
                   label=label[:3])
    sld.set(init); sld.pack(side="left", padx=2)
    bind_slider_and_entry(sld, ent)
    globals()[f"{base}_slider"] = sld
    globals()[f"{base}_field"]  = ent

# Column 1: R%, G%, B%
col1 = tk.Frame(top, bg="lightblue"); col1.pack(side="left", padx=5)
for ch in ("r","g","b"):
    f = tk.Frame(col1, bg="lightblue"); f.pack(pady=5)
    tk.Label(f, text=f"{ch.upper()}%:").pack(side="left")
    ent = tk.Entry(f, width=5); ent.insert(0,"100"); ent.pack(side="left", padx=2)
    sld = tk.Scale(f,
                   from_=0, to=200,
                   resolution=1,
                   length=150,
                   orient="horizontal",
                   label=ch.upper())
    sld.set(100); sld.pack(side="left", padx=2)
    bind_slider_and_entry(sld, ent)
    globals()[f"{ch}_slider"] = sld
    globals()[f"{ch}_field"]  = ent

# Column 2: Dither Palette
col2 = tk.Frame(top, bg="lightblue"); col2.pack(side="left", padx=5)
tk.Label(col2, text="Dither Palette").pack()
for i,name in enumerate(["C0","C1","C2","C3","C4","C5"]):
    f = tk.Frame(col2, bg="lightblue"); f.pack(pady=2)
    tk.Label(f, text=name).pack(side="left")
    for j in range(3):
        e = tk.Entry(f, width=4)
        e.insert(0, str(DEFAULT_DITHER_PALETTE[i][j]))
        e.pack(side="left", padx=1)
        dith_palette_entries[i][j] = e

# Column 3: E-Ink Palette
col3 = tk.Frame(top, bg="lightblue"); col3.pack(side="left", padx=5)
tk.Label(col3, text="E-Ink Palette").pack()
names = ["Blk","Wht","Ylw","Red","Blu","Grn"]
for i, nm in enumerate(names):
    f = tk.Frame(col3, bg="lightblue"); f.pack(pady=2)
    tk.Label(f, text=f"C{i}({nm})").pack(side="left")
    for j in range(3):
        e = tk.Entry(f, width=4)
        e.insert(0, str(DEFAULT_EINK_PALETTE[i][j]))
        e.pack(side="left", padx=1)
        eink_palette_entries[i][j] = e

# Column 4: Pipeline Buttons
col4 = tk.Frame(top, bg="lightblue"); col4.pack(side="left", padx=5)
tk.Button(col4, text="Load Image",    command=lambda: load_image()).pack(pady=5)
tk.Button(col4, text="Apply",         command=lambda: apply_palette()).pack(pady=5)
tk.Button(col4, text="Save E-Ink",command=lambda: save_eink()).pack(pady=5)
tk.Button(col4, text="Reset",         command=lambda: reset_all()).pack(pady=5)
tk.Button(col4, text="Flip Layout", command=toggle_layout).pack(pady=5)

# Column 5: All Settings Load/Save
col6 = tk.Frame(top, bg="lightblue"); col6.pack(side="left", padx=5)
tk.Label(col6, text="Settings").pack(pady=5)
tk.Button(col6, text="Save All", command=lambda: save_all_settings()).pack(pady=5)
tk.Button(col6, text="Load All", command=lambda: load_all_settings()).pack(pady=5)

# Column 6: zoom
col_zoom = tk.Frame(top, bg="lightblue")
col_zoom.pack(side="left", padx=5, pady=5)

tk.Label(col_zoom, text="Zoom %:").pack()
zoom_slider = tk.Scale(col_zoom,
                       from_=10, to=200,
                       orient="horizontal",
                       length=150,
                       command=lambda val: (update_left(), realtime_dither()))
zoom_slider.set(100)
zoom_slider.pack()



###################################
# BOTTOM FRAME: IMAGES + SYNC SLIDERS
###################################
bottom = tk.Frame(root, bg="lightgray")
bottom.pack(fill="both", expand=True)



pane_w = win_w//2 - 20
pane_h = win_h - 320 - 20

# Create the two scrollable image panes
left_pane  = ScrollableImage(bottom, pane_w, pane_h)
right_pane = ScrollableImage(bottom, pane_w, pane_h)
# configure for 1 row, 2 columns (always):
bottom.grid_rowconfigure(0, weight=1)
bottom.grid_columnconfigure(0, weight=1)
bottom.grid_columnconfigure(1, weight=1)

# use the mutable pane_w, pane_h:
left_pane  = ScrollableImage(bottom, pane_w, pane_h)
right_pane = ScrollableImage(bottom, pane_w, pane_h)

left_pane.grid(row=0, column=0, sticky="nsew", padx=5, pady=5)
right_pane.grid(row=0, column=1, sticky="nsew", padx=5, pady=5)
# — hide the right pane’s own scrollbars —  
right_pane.vbar.grid_remove()
right_pane.hbar.grid_remove()

def sync_y(*args):
    left_pane.canvas.yview(*args)
    right_pane.canvas.yview(*args)

def sync_x(*args):
    left_pane.canvas.xview(*args)
    right_pane.canvas.xview(*args)

left_pane.vbar.config(command=sync_y)
left_pane.hbar.config(command=sync_x)

left_pane.canvas.configure(
    yscrollcommand=lambda f, l: (left_pane.vbar.set(f, l),
                                 right_pane.canvas.yview_moveto(f)),
    xscrollcommand=lambda f, l: (left_pane.hbar.set(f, l),
                                 right_pane.canvas.xview_moveto(f))
)

left_pane.canvas.bind(
    "<MouseWheel>",
    lambda e: (
        left_pane.canvas.yview_scroll(int(-1*(e.delta//120)), "units"),
        right_pane.canvas.yview_scroll(int(-1*(e.delta//120)), "units")
    )
)


# Sync callbacks
def on_h_sync(val):
    frac = float(val)/100.0
    left_pane.canvas.xview_moveto(frac)
    right_pane.canvas.xview_moveto(frac)

def on_v_sync(val):
    frac = float(val)/100.0
    left_pane.canvas.yview_moveto(frac)
    right_pane.canvas.yview_moveto(frac)




###################################
# IMAGE UPDATE FUNCTIONS
###################################
def update_left():
    if 'original_img' not in globals(): return
    img = scale_to_target(original_img)

    # — apply display zoom —
    z = zoom_slider.get() / 100.0
    w, h = img.size
    img = img.resize((int(w*z), int(h*z)), Image.LANCZOS)

    left_pane.show(img)

def realtime_dither(*_):
    if 'original_img' not in globals(): return

    inv_g = read_val(inv_gamma_slider, inv_gamma_field)
    br    = read_val(brightness_slider, brightness_field)
    co    = read_val(contrast_slider,   contrast_field)
    rpct  = read_val(r_slider,          r_field)
    gpct  = read_val(g_slider,          g_field)
    bpct  = read_val(b_slider,          b_field)

    eink_img, _ = full_dither_pipeline(
        original_img, inv_g, br, co, rpct, gpct, bpct
    )

    # — apply display zoom —
    z = zoom_slider.get() / 100.0
    w, h = eink_img.size
    eink_img = eink_img.resize((int(w*z), int(h*z)), Image.LANCZOS)

    right_pane.show(eink_img)

###################################
# LOAD / SAVE / RESET CALLBACKS
###################################
def load_image():
    global original_img, image_path
    fn = filedialog.askopenfilename()
    if not fn: return
    image_path = fn
    original_img = Image.open(fn).convert("RGB")
    update_left()
    realtime_dither()

def apply_palette():
    realtime_dither()

def reset_all():
    inv_gamma_field.delete(0, tk.END); inv_gamma_field.insert(0,"1.0"); inv_gamma_slider.set(1.0)
    brightness_field.delete(0, tk.END); brightness_field.insert(0,"0"); brightness_slider.set(0)
    contrast_field.delete(0, tk.END); contrast_field.insert(0,"100"); contrast_slider.set(100)
    for fld,val in zip((r_field,g_field,b_field),(100,100,100)):
        fld.delete(0, tk.END); fld.insert(0,str(val))
    for s in (r_slider,g_slider,b_slider):
        s.set(100)
    for i in range(6):
        for j in range(3):
            dith_palette_entries[i][j].delete(0,tk.END)
            dith_palette_entries[i][j].insert(0,str(DEFAULT_DITHER_PALETTE[i][j]))
            eink_palette_entries[i][j].delete(0,tk.END)
            eink_palette_entries[i][j].insert(0,str(DEFAULT_EINK_PALETTE[i][j]))
    update_left()
    realtime_dither()





def save_eink():
    global is_landscape

    if 'original_img' not in globals():
        return

    # build paths & read controls
    base, _ = os.path.splitext(image_path)
    inv_g = read_val(inv_gamma_slider, inv_gamma_field)
    br    = read_val(brightness_slider, brightness_field)
    co    = read_val(contrast_slider,   contrast_field)
    rpct  = read_val(r_slider,          r_field)
    gpct  = read_val(g_slider,          g_field)
    bpct  = read_val(b_slider,          b_field)

    # 1) run your pipeline at 1200×1600
    eink_img, idx = full_dither_pipeline(
        original_img, inv_g, br, co, rpct, gpct, bpct
    )

    # 2) if landscape, rotate both image & index array
    if is_landscape:
        bmp_img = eink_img.rotate(90, expand=True)
        idx = np.rot90(idx, k=1)
    else:
        bmp_img = eink_img


    # 3) write RAW
    h, w = idx.shape
    raw_path = base + ".raw"
    with open(raw_path, "wb") as f:
        data = bytearray()
        for y in range(h):
            for x in range(0, w, 2):
                lv = idx[y, x]
                rv = idx[y, x+1]
                data.append((RAW_MAP.get(lv, 0) << 4) | RAW_MAP.get(rv, 0))
        f.write(data)

    
    print(f"Saved RAW  ⇒ {raw_path}")

def save_all_settings():
    fn = filedialog.asksaveasfilename(defaultextension=".txt")
    if not fn: return
    lines = []
    for fld in (inv_gamma_field, brightness_field, contrast_field,
                r_field, g_field, b_field):
        lines.append(fld.get())
    for c in build_dithering_palette():
        lines.append(f"{c[0]} {c[1]} {c[2]}")
    for c in build_eink_palette():
        lines.append(f"{c[0]} {c[1]} {c[2]}")
    with open(fn,"w") as f:
        f.write("\n".join(lines))
    print(f"All settings saved ⇒ {fn}")

def load_all_settings():
    fn = filedialog.askopenfilename()
    if not fn: return
    lines = open(fn).read().splitlines()
    if len(lines) < 18:
        print("Need 18 lines in file."); return
    for fld,val in zip(
        (inv_gamma_field, brightness_field, contrast_field,
         r_field, g_field, b_field),
        lines[:6]
    ):
        fld.delete(0,tk.END); fld.insert(0,val)
    inv_gamma_slider.set(float(lines[0])); brightness_slider.set(float(lines[1]))
    contrast_slider.set(float(lines[2])); r_slider.set(float(lines[3]))
    g_slider.set(float(lines[4])); b_slider.set(float(lines[5]))
    for i in range(6):
        r,g,b = lines[6+i].split()
        dith_palette_entries[i][0].delete(0,tk.END); dith_palette_entries[i][0].insert(0,r)
        dith_palette_entries[i][1].delete(0,tk.END); dith_palette_entries[i][1].insert(0,g)
        dith_palette_entries[i][2].delete(0,tk.END); dith_palette_entries[i][2].insert(0,b)
    for i in range(6):
        r,g,b = lines[12+i].split()
        eink_palette_entries[i][0].delete(0,tk.END); eink_palette_entries[i][0].insert(0,r)
        eink_palette_entries[i][1].delete(0,tk.END); eink_palette_entries[i][1].insert(0,g)
        eink_palette_entries[i][2].delete(0,tk.END); eink_palette_entries[i][2].insert(0,b)
    apply_palette(); print(f"All settings loaded ⇒ {fn}")

###################################
# START
###################################
root.mainloop()
