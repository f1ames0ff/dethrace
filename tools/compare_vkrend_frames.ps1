param(
    [string]$Reference,
    [string]$Candidate,
    [int]$Threshold = 8,
    [double]$MaxDiffFraction = 0.02
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir

function Resolve-ComparePath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return (Join-Path $repoRoot $Path)
}

function Read-Bmp24 {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)

    if ($bytes.Length -lt 54) {
        throw "file is too small to be a BMP ($($bytes.Length) bytes)"
    }
    if ($bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D) {
        throw "missing BM signature"
    }

    $dataOffset = [System.BitConverter]::ToUInt32($bytes, 10)
    $headerSize = [System.BitConverter]::ToUInt32($bytes, 14)
    $width = [System.BitConverter]::ToInt32($bytes, 18)
    $height = [System.BitConverter]::ToInt32($bytes, 22)
    $planes = [System.BitConverter]::ToUInt16($bytes, 26)
    $bits = [System.BitConverter]::ToUInt16($bytes, 28)
    $compression = [System.BitConverter]::ToUInt32($bytes, 30)

    if ($headerSize -lt 40) {
        throw "unsupported header size $headerSize (BITMAPINFOHEADER or later required)"
    }
    if ($planes -ne 1) {
        throw "unsupported plane count $planes"
    }
    if ($bits -ne 24) {
        throw "unsupported bit depth $bits (24-bit only)"
    }
    if ($compression -ne 0) {
        throw "unsupported compression $compression (uncompressed BI_RGB only)"
    }
    if ($width -le 0 -or $height -eq 0) {
        throw "unsupported dimensions ${width}x${height}"
    }

    $topDown = $height -lt 0
    if ($topDown) {
        $height = -$height
    }

    $stride = ([int][math]::Floor(($width * 3 + 3) / 4.0)) * 4
    if ($bytes.Length -lt ($dataOffset + $stride * $height)) {
        throw "pixel data is truncated ($($bytes.Length) bytes, need $($dataOffset + $stride * $height))"
    }

    return [pscustomobject]@{
        Bytes   = $bytes
        Offset  = [int]$dataOffset
        Width   = $width
        Height  = $height
        TopDown = $topDown
        Stride  = $stride
    }
}

$inputErrors = @()

if ($Reference -eq "") {
    $inputErrors += "Reference path is empty."
} elseif (-not (Test-Path -LiteralPath (Resolve-ComparePath -Path $Reference) -PathType Leaf)) {
    $inputErrors += "Reference image not found: $(Resolve-ComparePath -Path $Reference)"
} else {
    $Reference = (Resolve-Path -LiteralPath (Resolve-ComparePath -Path $Reference)).Path
}

if ($Candidate -eq "") {
    $inputErrors += "Candidate path is empty."
} elseif (-not (Test-Path -LiteralPath (Resolve-ComparePath -Path $Candidate) -PathType Leaf)) {
    $inputErrors += "Candidate image not found: $(Resolve-ComparePath -Path $Candidate)"
} else {
    $Candidate = (Resolve-Path -LiteralPath (Resolve-ComparePath -Path $Candidate)).Path
}

if ($Threshold -lt 0) {
    $inputErrors += "Threshold must be zero or greater (got $Threshold)."
}

if ($MaxDiffFraction -lt 0 -or $MaxDiffFraction -gt 1) {
    $inputErrors += "MaxDiffFraction must be between 0 and 1 (got $MaxDiffFraction)."
}

if ($inputErrors.Count -gt 0) {
    foreach ($message in $inputErrors) {
        Write-Host "ERROR: $message"
    }
    exit 2
}

try {
    $referenceImage = Read-Bmp24 -Path $Reference
} catch {
    Write-Host "ERROR: cannot read reference image $Reference : $($_.Exception.Message)"
    exit 2
}

try {
    $candidateImage = Read-Bmp24 -Path $Candidate
} catch {
    Write-Host "ERROR: cannot read candidate image $Candidate : $($_.Exception.Message)"
    exit 2
}

try {
    Add-Type -TypeDefinition @"
using System;

namespace VkrendFrameCompare
{
    public sealed class FrameDifference
    {
        public int Width;
        public int Height;
        public double[] MeanAbsolute = new double[3];
        public double[] AboveThresholdFraction = new double[3];
        public double[] MaxAbsolute = new double[3];
        public double PixelDifferenceFraction;
        public long PixelsAboveThreshold;
        public double[] Grid = new double[16 * 12];
    }

    public static class FrameComparer
    {
        public static FrameDifference Compare(byte[] reference, int referenceOffset, int referenceStride, bool referenceTopDown,
            byte[] candidate, int candidateOffset, int candidateStride, bool candidateTopDown,
            int width, int height, int threshold)
        {
            FrameDifference result = new FrameDifference();
            result.Width = width;
            result.Height = height;

            long[] channelSum = new long[3];
            long[] channelAbove = new long[3];
            long[] channelMax = new long[3];
            long[] gridCount = new long[16 * 12];
            double[] gridSum = new double[16 * 12];
            long pixelsAbove = 0;

            for (int y = 0; y < height; y++)
            {
                int referenceRow = referenceTopDown ? y : (height - 1 - y);
                int candidateRow = candidateTopDown ? y : (height - 1 - y);
                int referenceBase = referenceOffset + referenceRow * referenceStride;
                int candidateBase = candidateOffset + candidateRow * candidateStride;
                int gridRow = (int)((long)y * 12 / height);

                for (int x = 0; x < width; x++)
                {
                    int referencePixel = referenceBase + x * 3;
                    int candidatePixel = candidateBase + x * 3;

                    int deltaBlue = reference[referencePixel] - candidate[candidatePixel];
                    int deltaGreen = reference[referencePixel + 1] - candidate[candidatePixel + 1];
                    int deltaRed = reference[referencePixel + 2] - candidate[candidatePixel + 2];

                    if (deltaBlue < 0)
                        deltaBlue = -deltaBlue;
                    if (deltaGreen < 0)
                        deltaGreen = -deltaGreen;
                    if (deltaRed < 0)
                        deltaRed = -deltaRed;

                    channelSum[0] += deltaRed;
                    channelSum[1] += deltaGreen;
                    channelSum[2] += deltaBlue;

                    if (deltaRed > channelMax[0])
                        channelMax[0] = deltaRed;
                    if (deltaGreen > channelMax[1])
                        channelMax[1] = deltaGreen;
                    if (deltaBlue > channelMax[2])
                        channelMax[2] = deltaBlue;

                    if (deltaRed > threshold)
                        channelAbove[0]++;
                    if (deltaGreen > threshold)
                        channelAbove[1]++;
                    if (deltaBlue > threshold)
                        channelAbove[2]++;

                    if (deltaRed > threshold || deltaGreen > threshold || deltaBlue > threshold)
                        pixelsAbove++;

                    int cell = gridRow * 16 + (int)((long)x * 16 / width);
                    gridSum[cell] += (deltaRed + deltaGreen + deltaBlue) / 3.0;
                    gridCount[cell]++;
                }
            }

            long pixels = (long)width * height;

            for (int channel = 0; channel < 3; channel++)
            {
                result.MeanAbsolute[channel] = (double)channelSum[channel] / pixels;
                result.AboveThresholdFraction[channel] = (double)channelAbove[channel] / pixels;
                result.MaxAbsolute[channel] = channelMax[channel];
            }

            result.PixelsAboveThreshold = pixelsAbove;
            result.PixelDifferenceFraction = (double)pixelsAbove / pixels;

            for (int cell = 0; cell < 16 * 12; cell++)
            {
                if (gridCount[cell] > 0)
                    result.Grid[cell] = gridSum[cell] / gridCount[cell];
                else
                    result.Grid[cell] = -1.0;
            }

            return result;
        }
    }
}
"@
} catch {
    Write-Host "ERROR: failed to load the comparison helper: $($_.Exception.Message)"
    exit 2
}

Write-Host "==== VKREND FRAME COMPARE ===="
Write-Host ("Reference        : {0}" -f $Reference)
Write-Host ("Candidate        : {0}" -f $Candidate)
Write-Host ("Reference size   : {0}x{1}" -f $referenceImage.Width, $referenceImage.Height)
Write-Host ("Candidate size   : {0}x{1}" -f $candidateImage.Width, $candidateImage.Height)

$sizesMatch = ($referenceImage.Width -eq $candidateImage.Width) -and ($referenceImage.Height -eq $candidateImage.Height)
if ($sizesMatch) {
    Write-Host "Sizes match      : yes"
} else {
    Write-Host "Sizes match      : no"
    Write-Host "ERROR: image sizes differ; the frames cannot be compared."
    Write-Host "=============================="
    exit 2
}

$difference = [VkrendFrameCompare.FrameComparer]::Compare(
    $referenceImage.Bytes, $referenceImage.Offset, $referenceImage.Stride, $referenceImage.TopDown,
    $candidateImage.Bytes, $candidateImage.Offset, $candidateImage.Stride, $candidateImage.TopDown,
    $referenceImage.Width, $referenceImage.Height, $Threshold)

$channelNames = @("Red  ", "Green", "Blue ")

Write-Host ("Threshold        : {0} per channel" -f $Threshold)
Write-Host ("Pixels compared  : {0}" -f ($difference.Width * $difference.Height))
Write-Host ""
Write-Host "Per-channel differences (0-255):"
for ($channel = 0; $channel -lt 3; $channel++) {
    Write-Host ("  {0} : mean |diff| {1,7:0.000}   above threshold {2,6:0.000}%   max {3,3:0}" -f `
        $channelNames[$channel], $difference.MeanAbsolute[$channel],
        ($difference.AboveThresholdFraction[$channel] * 100.0), $difference.MaxAbsolute[$channel])
}
Write-Host ""
Write-Host ("Pixels differing (> {0} on any channel): {1} ({2:0.0000}%)" -f `
    $Threshold, $difference.PixelsAboveThreshold, ($difference.PixelDifferenceFraction * 100.0))
Write-Host ""
Write-Host "Grid (16 columns x 12 rows, mean |diff| per cell, 0-255; row 0 is the top of the image):"
$header = "     "
for ($column = 0; $column -lt 16; $column++) {
    $header += ("{0,4:0}" -f $column)
}
Write-Host $header
for ($row = 0; $row -lt 12; $row++) {
    $line = ("r{0,2}  " -f $row)
    for ($column = 0; $column -lt 16; $column++) {
        $value = $difference.Grid[$row * 16 + $column]
        if ($value -lt 0) {
            $line += "   ."
        } else {
            $line += ("{0,4:0}" -f $value)
        }
    }
    Write-Host $line
}
Write-Host ""

$pass = $difference.PixelDifferenceFraction -le $MaxDiffFraction

Write-Host ("MaxDiffFraction  : {0:0.0000} ({1:0.00}% of pixels)" -f $MaxDiffFraction, ($MaxDiffFraction * 100.0))
if ($pass) {
    Write-Host "Result           : MATCH"
} else {
    Write-Host "Result           : MISMATCH"
    Write-Host ("NOTE: {0:0.0000}% of pixels differ above the threshold, over the {1:0.0000}% limit." -f `
        ($difference.PixelDifferenceFraction * 100.0), ($MaxDiffFraction * 100.0))
}
Write-Host "=============================="

if ($pass) {
    exit 0
}
exit 1
