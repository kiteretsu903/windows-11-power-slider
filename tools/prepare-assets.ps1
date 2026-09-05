$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path $PSScriptRoot -Parent
# Generated source art is kept intact. Convert its neutral matte to alpha for
# native icon packaging; the app never draws its own substitute artwork.
foreach($name in 'power','battery','coffee') {
    $source=[Drawing.Bitmap]::new((Join-Path $root "assets\$name.png"))
    $out=[Drawing.Bitmap]::new(256,256,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g=[Drawing.Graphics]::FromImage($out)
    $g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $factor=256.0/[Math]::Max($source.Width,$source.Height)
    $width=[int]($source.Width*$factor);$height=[int]($source.Height*$factor)
    $g.DrawImage($source,[int]((256-$width)/2),[int]((256-$height)/2),$width,$height);$g.Dispose();$source.Dispose()
    for($y=0;$y -lt 256;$y++) {
        for($x=0;$x -lt 256;$x++) {
            $c=$out.GetPixel($x,$y)
            $chroma=[Math]::Max($c.R,[Math]::Max($c.G,$c.B))-[Math]::Min($c.R,[Math]::Min($c.G,$c.B))
            $alpha=[int]([Math]::Clamp(($chroma-8)*255/28,0,255)*$c.A/255)
            $out.SetPixel($x,$y,[Drawing.Color]::FromArgb($alpha,$c.R,$c.G,$c.B))
        }
    }
    $out.Save((Join-Path $root "assets\$name-ui.png"),[Drawing.Imaging.ImageFormat]::Png)
    $out.Dispose()
}
# The approved product artwork is independent from the card illustrations.
# Its prebuilt, DPI-complete ICO is committed so normal builds need no Node.js.
# When changing that artwork, regenerate it with tools/export-main-icon.cjs.
if(-not (Test-Path -LiteralPath (Join-Path $root 'src\PowerModeNative.ico'))) {
    throw 'Missing application ICO. Run node tools/export-main-icon.cjs (requires sharp).'
}
