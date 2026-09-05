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
# The product icon is independent from the plugged-in card artwork.
# Do not regenerate the launcher/installer icon from power-ui.png.
$source=[Drawing.Bitmap]::new((Join-Path $root 'assets\app-icon.png'))
$images=[Collections.Generic.List[byte[]]]::new()
$sizes=@(16,20,24,32,40,48,64,128,256)
foreach($size in $sizes) {
    $b=[Drawing.Bitmap]::new($size,$size,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g=[Drawing.Graphics]::FromImage($b);$g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
    $g.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($source,0,0,$size,$size);$g.Dispose()
    $s=[IO.MemoryStream]::new();$b.Save($s,[Drawing.Imaging.ImageFormat]::Png)
    $images.Add($s.ToArray());$s.Dispose();$b.Dispose()
}
# Keep the website and README logo in sync with the 256px ICO frame.
[IO.File]::WriteAllBytes((Join-Path $root 'site\assets\icon.png'),$images[$images.Count-1])
[IO.File]::WriteAllBytes((Join-Path $root 'docs\images\app-icon.png'),$images[$images.Count-1])
$source.Dispose()
$w=[IO.BinaryWriter]::new([IO.File]::Create((Join-Path $root 'src\PowerModeNative.ico')))
try {
    $w.Write([uint16]0);$w.Write([uint16]1);$w.Write([uint16]$sizes.Count)
    $offset=6+16*$sizes.Count
    for($i=0;$i -lt $sizes.Count;$i++) {
        $dimension=if($sizes[$i] -eq 256){0}else{$sizes[$i]}
        $w.Write([byte]$dimension);$w.Write([byte]$dimension);$w.Write([byte]0);$w.Write([byte]0)
        $w.Write([uint16]1);$w.Write([uint16]32);$w.Write([uint32]$images[$i].Length);$w.Write([uint32]$offset)
        $offset+=$images[$i].Length
    }
    foreach($bytes in $images){$w.Write($bytes)}
} finally {$w.Dispose()}
