Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile("$PSScriptRoot\debug_tex_0.png")
Write-Output ("Size: {0} x {1}" -f $bmp.Width, $bmp.Height)
$rSum = 0; $gSum = 0; $bSum = 0; $count = 0
$minR = 255; $maxR = 0
for ($y = 0; $y -lt $bmp.Height; $y += 25) {
    for ($x = 0; $x -lt $bmp.Width; $x += 25) {
        $p = $bmp.GetPixel($x, $y)
        $rSum += $p.R; $gSum += $p.G; $bSum += $p.B; $count++
        if ($p.R -lt $minR) { $minR = $p.R }
        if ($p.R -gt $maxR) { $maxR = $p.R }
    }
}
Write-Output ("Avg R={0:N1} G={1:N1} B={2:N1} samples={3} minR={4} maxR={5}" -f ($rSum / $count), ($gSum / $count), ($bSum / $count), $count, $minR, $maxR)
$bmp.Dispose()
