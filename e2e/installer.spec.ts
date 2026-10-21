import { test, expect } from '@playwright/test';
import fs from 'fs';
import path from 'path';

test.describe('ESP Web Installer', () => {
  test('should display the web install button', async ({ page }) => {
    // Caricamento del file locale
    const indexPath = `file:///${path.resolve(__dirname, '../web-installer/index.html').replace(/\\/g, '/')}`;
    
    await page.goto(indexPath);
    
    // Un bottone per firmware: ESP32-S3 (Wi-Fi) alla radice, i due ESP32-P4 (Ethernet)
    // nelle cartelle create da release.yml
    const installButtons = page.locator('esp-web-install-button');
    await expect(installButtons).toHaveCount(3);
    // Non testiamo toBeVisible() perchè il custom element web-install-button 
    // ha una sua logica shadow DOM e potrebbe richiedere script asincroni
    
    // Verifica dell'attributo manifest
    await expect(installButtons.nth(0)).toHaveAttribute('manifest', 'manifest.json');
    await expect(installButtons.nth(1)).toHaveAttribute('manifest', 'p4/manifest.json');
    await expect(installButtons.nth(2)).toHaveAttribute('manifest', 'p4-v1/manifest.json');
  });

  test('manifest.json is valid and contains correct offsets', () => {
    const manifestPath = path.resolve(__dirname, '../web-installer/manifest.json');
    const content = fs.readFileSync(manifestPath, 'utf-8');
    const manifest = JSON.parse(content);
    
    expect(manifest.name).toBe('ESP32 NUT Server');
    expect(manifest.builds[0].chipFamily).toBe('ESP32-S3');
    
    const parts = manifest.builds[0].parts;
    expect(parts.find((p: any) => p.path === 'bootloader.bin').offset).toBe(0);
    expect(parts.find((p: any) => p.path === 'partitions.bin').offset).toBe(32768);
    expect(parts.find((p: any) => p.path === 'firmware.bin').offset).toBe(65536);
  });
});
